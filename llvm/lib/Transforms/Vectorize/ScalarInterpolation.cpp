//
// Created by reza on 18/09/23.
//

#include "ScalarInterpolation.h"
#include "VPlanCFG.h"

//#define DEBUG_TYPE "scalar-interpolation"

using namespace llvm;

void ScalarInterpolationCostModel::addFeature(Instruction *I, Type *T)
{
  auto Op = I->getOpcodeName();
  if (InstructionTypeMap.find(Op) == InstructionTypeMap.end())
    InstructionTypeMap[Op] = DenseMap<Type *, unsigned>();
  if (InstructionTypeMap[Op].find(T) == InstructionTypeMap[Op].end())
    InstructionTypeMap[Op][T] = 0;
  InstructionTypeMap[Op][T]++;
}

void ScalarInterpolationCostModel::extractFeaturesFromLoop(llvm::Loop *L) {
  RecurrenceDescriptor RedDes;
  for (auto *BB : L->blocks())
  {
    NumberOfInstructions += BB->size();
    for (auto &I : *BB)
    {
      addFeature(&I, I.getType());
      if (auto *Phi = dyn_cast<PHINode>(&I))
        HasReductions = HasReductions || RecurrenceDescriptor::isReductionPHI(Phi, L, RedDes);
      if (I.isBinaryOp() || I.isUnaryOp())
        NumberOfComputeInstructions++;
      if (I.mayReadOrWriteMemory())
        NumberOfMemoryInstructions++;
    }
  }
  MemToComputeRatio = (float)NumberOfMemoryInstructions / (float)NumberOfComputeInstructions;
}

unsigned ScalarInterpolationCostModel::getValueFromMap(const char* I, Type *T) {
  if (InstructionTypeMap.find(I) == InstructionTypeMap.end())
    return 0;
  auto Map = InstructionTypeMap[I];
  if (Map.find(T) == Map.end())
    return 0;
  return Map[T];
}

unsigned ScalarInterpolationCostModel::getProfitableSIFactor(Loop *OrigLoop) {
  Type* I32 = Type::getInt32Ty(OrigLoop->getHeader()->getContext());
  Type* I64 = Type::getInt64Ty(OrigLoop->getHeader()->getContext());
  Type* Void = Type::getVoidTy(OrigLoop->getHeader()->getContext());
  Type* Ptr = Type::getInt8PtrTy(OrigLoop->getHeader()->getContext());
  I64->getTypeID();
  extractFeaturesFromLoop(OrigLoop);
  if (HasReductions) {
    if (getValueFromMap("mul", I32) == 0) {
      if (getValueFromMap("load", I32) != 0) {
        if (getValueFromMap("or", I64) == 0) {
          return 4;
        } else { // or != 0
          return (NumberOfInstructions <= 16) ? 8 : 4;
        }
      }
      return 8;
    } else { // mul != 0
      if (getValueFromMap("add", I64) <= 1) {
        if (MemToComputeRatio < 0.24) {
          return (getValueFromMap("or", I32) == 0) ? 8 : 4;
        } else { // MemToComputeRatio >= 0.24
          return (getValueFromMap("store", Void) == 0) ? 8 : 2;
        }
      } else { // add.i64 > 1
        if (getValueFromMap("add", I32) <= 1)
          return (getValueFromMap("mul", I32) <= 1) ? 4 : 8;
        return 4;
      }
    }
  } else { // Does not have reductions
    if (getValueFromMap("mul", I32) == 0) {
      if (NumberOfInstructions <= 8) {
        if (getValueFromMap("store", Void) == 0) {
          return 2;
        } else { // store != 0
          return (MemToComputeRatio < 0.75) ? 0 : 2;
        }
      } else { // NumberOfInstructions > 8
        if (getValueFromMap("or", I64) == 0) {
          return (getValueFromMap("add", I64) <= 2) ? 2 : 0;
        } else { // or.i64 != 0
          return (getValueFromMap("getelementptr", Ptr) <= 2) ? 4 : 8;
        }
      }
    } else { // mul.i32 != 0
      if (NumberOfInstructions < 20) {
        if (getValueFromMap("add", I32) == 0)
          return 2;
        return (getValueFromMap("store", Void) <= 1) ? 4 : 8;
      } else { // NumberOfInstructions >= 20
        if (MemToComputeRatio <= 0.68)
          return (getValueFromMap("and", I32) == 0) ? 2 : 4;
        return (getValueFromMap("xor", I32) == 0) ? 8 : 1;
      }
    }
  }
}

bool ScalarInterpolationCostModel::hasFloatingPointInstruction(VPBasicBlock *VPBB) {
  return any_of(*VPBB, [&](VPRecipeBase &R) {
    auto* Instr = getUnderlyingInstructionOfRecipe(R);
    if (!Instr)
      return false;
    return Instr->getType()->isFloatingPointTy();
  });
}

bool ScalarInterpolationCostModel::isLegalToInterpolate(llvm::VPlan &Plan) {
  if (hasNonInterpolatableRecipe(Plan)) {
    LLVM_DEBUG(dbgs() << "LV(SI): Not legal to interpolate due to non-interpolatable recipe\n");
    return false;
  }
  ReversePostOrderTraversal<VPBlockDeepTraversalWrapper<VPBlockBase *>> RPOT(
      Plan.getEntry());
  for (VPBasicBlock *VPBB: reverse(VPBlockUtils::blocksOnly<VPBasicBlock>(RPOT))) {
    if (hasFloatingPointInstruction(VPBB)) {
      LLVM_DEBUG(dbgs() << "LV(SI): Not legal to interpolate due to floating point instructions\n");
      return false;
    }
  }
  return true;
}

bool ScalarInterpolationCostModel::hasNonInterpolatableRecipe(llvm::VPlan &Plan) {
  ReversePostOrderTraversal<VPBlockDeepTraversalWrapper<VPBlockBase *>> RPOT(
      Plan.getEntry());
  for (VPBasicBlock *VPBB: reverse(VPBlockUtils::blocksOnly<VPBasicBlock>(RPOT))) {
    if (any_of(*VPBB, [&](VPRecipeBase &R) {
          return any_of(NonInterpolatableRecipes, [&R](VPDef::VPRecipeTy &NIR) {
                return R.getVPDefID() == NIR;
              });
        })) {
      return true;
    }
  }
  return false;
}

unsigned ScalarInterpolationCostModel::getProfitableSIFactor(VPlan &Plan, unsigned int UserIC, unsigned UserSI, unsigned MaxSafeElements, bool IsScalarInterpolationEnabled) {
  if (!isLegalToInterpolate(Plan))
    return 0;
  unsigned SuggestedSI = (IsScalarInterpolationEnabled && UserSI == 0)
                             ? getSIFactor(Plan, UserIC) : UserSI;
  LLVM_DEBUG(
        dbgs() << "LV(SI):" << (IsScalarInterpolationEnabled ? " SI enabled" : "SI disabled") << ", UserSI is " << UserSI << "\n"
  );
  unsigned MaxSI = Plan.getMaximumSIF(MaxSafeElements);
  LLVM_DEBUG(
        dbgs() << "LV(SI): SuggestedSI=" << SuggestedSI << ", MaxSI=" << MaxSI
                 << "\n"
  );
  return MaxSI < SuggestedSI ? 0 : SuggestedSI;
}

Instruction *ScalarInterpolationCostModel::getUnderlyingInstructionOfRecipe(VPRecipeBase &R) {
  Instruction *Instr = nullptr;
  if (isa<VPWidenMemoryInstructionRecipe>(R)) {
    auto *WidenMemInstr = cast<VPWidenMemoryInstructionRecipe>(&R);
    if (WidenMemInstr->isStore()) {
      Instr = &WidenMemInstr->getIngredient();
    }
  }
  if (!Instr) {
    if (R.definedValues().empty() || !R.getVPValue(0)->getUnderlyingValue())
      return nullptr;
    Instr = R.getUnderlyingInstr();
  }
  return Instr;
}

SmallVector<int, 6> ResourceHandlerX86::getScalarResourcesFor(Instruction& Instr) {
  switch (Instr.getOpcode()) {
  // Terminators
//  case Ret:    return "ret";
  case Instruction::IndirectBr:
  case Instruction::Br:     return {0, 6};
//  // Standard binary operators...
  case Instruction::Trunc:
  case Instruction::ZExt:
  case Instruction::SExt:
  case Instruction::And:
  case Instruction::Or:
  case Instruction::Xor:
  case Instruction::Add:
  case Instruction::Shl:
  case Instruction::LShr:
  case Instruction::AShr:
  case Instruction::ICmp:
  case Instruction::Sub: return {0, 1, 5, 6};
  case Instruction::Mul: return {1};
  case Instruction::UDiv:
  case Instruction::SDiv:
  case Instruction::URem:
  case Instruction::SRem: return {0};
  case Instruction::Load: return {2, 3};
  case Instruction::Store: return {4};
  default: return {};
//
//  // Memory instructions...
//  case Instruction::Alloca:        return "alloca";
//  case Instruction::AtomicCmpXchg: return "cmpxchg";
//  case Instruction::AtomicRMW:     return "atomicrmw";
//  case Instruction::Fence:         return "fence";
//  case Instruction::GetElementPtr: return "getelementptr";
//
//  // Convert instructions...

//  case Instruction::FPTrunc:       return "fptrunc";
//  case Instruction::FPExt:         return "fpext";
//  case Instruction::FPToUI:        return "fptoui";
//  case Instruction::FPToSI:        return "fptosi";
//  case Instruction::UIToFP:        return "uitofp";
//  case Instruction::SIToFP:        return "sitofp";
//  case Instruction::IntToPtr:      return "inttoptr";
//  case Instruction::PtrToInt:      return "ptrtoint";
//  case Instruction::BitCast:       return "bitcast";
//  case Instruction::AddrSpaceCast: return "addrspacecast";
//
//  // Other instructions...
//  case Instruction::ICmp:           return "icmp";
//  case Instruction::FCmp:           return "fcmp";
//  case Instruction::PHI:            return "phi";
//  case Instruction::Select:         return "select";
//  case Instruction::Call:           return "call";

//  case Instruction::VAArg:          return "va_arg";
//  case Instruction::ExtractElement: return "extractelement";
//  case Instruction::InsertElement:  return "insertelement";
//  case Instruction::ShuffleVector:  return "shufflevector";
//  case Instruction::ExtractValue:   return "extractvalue";
//  case Instruction::InsertValue:    return "insertvalue";
//  case Instruction::LandingPad:     return "landingpad";
//  case Instruction::CleanupPad:     return "cleanuppad";
//  case Instruction::Freeze:         return "freeze";
//
  }
}

SmallVector<int, 6> ResourceHandlerX86::getVectorResourcesFor(Instruction& Instr) {
  switch (Instr.getOpcode()) {
  case Instruction::IndirectBr:
  case Instruction::Br: return {0, 6};
  case Instruction::Trunc:
  case Instruction::ZExt:
  case Instruction::SExt:
  case Instruction::And:
  case Instruction::Or :
  case Instruction::Xor:
  case Instruction::Add:
  case Instruction::Shl:
  case Instruction::LShr:
  case Instruction::AShr:
  case Instruction::ICmp:
  case Instruction::Sub: return {0, 1, 5};
  case Instruction::Mul: return {0, 1};
  case Instruction::UDiv:
  case Instruction::SDiv:
  case Instruction::URem:
  case Instruction::SRem: return {0};
  case Instruction::Load: return {2, 3};
  case Instruction::Store: return {4};
  case Instruction::ShuffleVector: return {5};
  default: return {};
  }
}

SmallVector<int, 6> ResourceHandlerTSV110::getScalarResourcesFor(Instruction& Instr) {
  switch (Instr.getOpcode()) {
    // Terminators
    //  case Ret:    return "ret";
  case Instruction::IndirectBr:
  case Instruction::Br:     return {1, 2};
    //  // Standard binary operators...
  case Instruction::Trunc:
  case Instruction::ZExt:
  case Instruction::SExt:
  case Instruction::And:
  case Instruction::Or :
  case Instruction::Xor:
  case Instruction::Add:
  case Instruction::Shl:
  case Instruction::LShr:
  case Instruction::AShr:
  case Instruction::Select:
  case Instruction::Sub: return {0, 1, 2};
  case Instruction::Mul:
  case Instruction::UDiv:
  case Instruction::SDiv:
  case Instruction::URem:
  case Instruction::SRem: return {3};
  case Instruction::Load:
  case Instruction::Store: return {6, 7};
  case Instruction::ICmp: return {1};
  default: return {};
    //
    //  // Memory instructions...
    //  case Instruction::Alloca:        return "alloca";
    //  case Instruction::AtomicCmpXchg: return "cmpxchg";
    //  case Instruction::AtomicRMW:     return "atomicrmw";
    //  case Instruction::Fence:         return "fence";
    //  case Instruction::GetElementPtr: return "getelementptr";
    //
    //  // Convert instructions...

    //  case Instruction::FPTrunc:       return "fptrunc";
    //  case Instruction::FPExt:         return "fpext";
    //  case Instruction::FPToUI:        return "fptoui";
    //  case Instruction::FPToSI:        return "fptosi";
    //  case Instruction::UIToFP:        return "uitofp";
    //  case Instruction::SIToFP:        return "sitofp";
    //  case Instruction::IntToPtr:      return "inttoptr";
    //  case Instruction::PtrToInt:      return "ptrtoint";
    //  case Instruction::BitCast:       return "bitcast";
    //  case Instruction::AddrSpaceCast: return "addrspacecast";
    //
    //  // Other instructions...
    //  case Instruction::FCmp:           return "fcmp";
    //  case Instruction::PHI:            return "phi";
    //  case Instruction::Select:         return "select";
    //  case Instruction::Call:           return "call";

    //  case Instruction::VAArg:          return "va_arg";
    //  case Instruction::ExtractElement: return "extractelement";
    //  case Instruction::InsertElement:  return "insertelement";
    //  case Instruction::ShuffleVector:  return "shufflevector";
    //  case Instruction::ExtractValue:   return "extractvalue";
    //  case Instruction::InsertValue:    return "insertvalue";
    //  case Instruction::LandingPad:     return "landingpad";
    //  case Instruction::CleanupPad:     return "cleanuppad";
    //  case Instruction::Freeze:         return "freeze";
    //
  }
}

SmallVector<int, 6> ResourceHandlerTSV110::getVectorResourcesFor(Instruction& Instr) {
  switch (Instr.getOpcode()) {
    // Terminators
    //  case Ret:    return "ret";
  case Instruction::IndirectBr:
  case Instruction::Br:     return {1, 2};
    //  // Standard binary operators...
  case Instruction::Trunc:
  case Instruction::ZExt:
  case Instruction::SExt:
  case Instruction::And:
  case Instruction::Or :
  case Instruction::Xor:
  case Instruction::Add:
  case Instruction::ICmp:
  case Instruction::Sub: return {4, 5};
  case Instruction::Shl:
  case Instruction::LShr:
  case Instruction::AShr:
  case Instruction::Mul: return {4};
  case Instruction::UDiv:
  case Instruction::SDiv:
  case Instruction::URem:
  case Instruction::SRem: return {3};
  case Instruction::Load:
  case Instruction::Store: return {6, 7};
  default: return {};
    //
    //  // Memory instructions...
    //  case Instruction::Alloca:        return "alloca";
    //  case Instruction::AtomicCmpXchg: return "cmpxchg";
    //  case Instruction::AtomicRMW:     return "atomicrmw";
    //  case Instruction::Fence:         return "fence";
    //  case Instruction::GetElementPtr: return "getelementptr";
    //
    //  // Convert instructions...

    //  case Instruction::FPTrunc:       return "fptrunc";
    //  case Instruction::FPExt:         return "fpext";
    //  case Instruction::FPToUI:        return "fptoui";
    //  case Instruction::FPToSI:        return "fptosi";
    //  case Instruction::UIToFP:        return "uitofp";
    //  case Instruction::SIToFP:        return "sitofp";
    //  case Instruction::IntToPtr:      return "inttoptr";
    //  case Instruction::PtrToInt:      return "ptrtoint";
    //  case Instruction::BitCast:       return "bitcast";
    //  case Instruction::AddrSpaceCast: return "addrspacecast";
    //
    //  // Other instructions...
    //  case Instruction::ICmp:           return "icmp";
    //  case Instruction::FCmp:           return "fcmp";
    //  case Instruction::PHI:            return "phi";
    //  case Instruction::Select:         return "select";
    //  case Instruction::Call:           return "call";

    //  case Instruction::VAArg:          return "va_arg";
    //  case Instruction::ExtractElement: return "extractelement";
    //  case Instruction::InsertElement:  return "insertelement";
    //  case Instruction::ShuffleVector:  return "shufflevector";
    //  case Instruction::ExtractValue:   return "extractvalue";
    //  case Instruction::InsertValue:    return "insertvalue";
    //  case Instruction::LandingPad:     return "landingpad";
    //  case Instruction::CleanupPad:     return "cleanuppad";
    //  case Instruction::Freeze:         return "freeze";
    //
  }
}

SmallVector<int, 6> ResourceHandler::getResourcesFor(llvm::Instruction &Instr, bool isVector) {
  return isVector ? getVectorResourcesFor(Instr) : getScalarResourcesFor(Instr);
}

void ResourceHandler::initialize(SmallVector<SmallVector<int>> PortUniqueness) {
  for (auto Port: PortUniqueness) {
    Resources.push_back(true);
    float UniquenessScore = 0;
    for (auto S: Port)
      UniquenessScore += S;
    Priorities.push_back(PortUniqueness.size() * Port.size() - UniquenessScore);
  }
}

int ResourceHandler::scheduleInstructionOnResource(Instruction& Instr, bool IsVector, float RandomWeight) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0.0, 1.0);
  SmallVector<int, 6> AvailableResources;
  for (auto Resource : getResourcesFor(Instr, IsVector))
    if (isResourceAvailable(Resource))
      AvailableResources.push_back(Resource);
  int SelectedResource = -1;
  float MaxScore = 0;
  for (auto Resource : AvailableResources) {
    float Score = (1 - RandomWeight) / Priorities[Resource] + RandomWeight * dis(gen);
    if (Score > MaxScore) {
      MaxScore = Score;
      SelectedResource = Resource;
    }
  }
  setResourceUnavailable(SelectedResource);
  return SelectedResource;
}

bool ResourceHandler::isResourceAvailableFor(Instruction& Instr, bool isVector) {
  if (!getResourcesFor(Instr, isVector).size())
    return true;
  return any_of(getResourcesFor(Instr, isVector), [&](int Resource) {
    return isResourceAvailable(Resource);
  });
}
