//
// Created by reza on 18/09/23.
//

#include "ScalarInterpolation.h"
#include "VPlanCFG.h"

#define DEBUG_TYPE "scalar-interpolation"

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

unsigned ScalarInterpolationCostModel::getProfitableSIFactor(VPlan &Plan, Loop *OrigLoop, unsigned UserSI, unsigned MaxSafeElements, bool IsScalarInterpolationEnabled) {
  if (hasNonInterpolatableRecipe(Plan)) {
    return 0;
  }
  unsigned SuggestedSI = (IsScalarInterpolationEnabled)
                             ? getProfitableSIFactor(OrigLoop) : 0;
  unsigned MaxSI = Plan.getMaximumSIF(MaxSafeElements);
  if (MaxSI < UserSI) {
    return MaxSI < SuggestedSI ? 0 : SuggestedSI;
  }
  if (UserSI == 0)
    return SuggestedSI;
  return UserSI;
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
