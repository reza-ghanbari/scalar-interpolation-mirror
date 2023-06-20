//
// Created by reza on 29/05/23.
//

#include "ScalarInterpolation.h"
#include "LoopVectorizationPlanner.h"
#include "VPRecipeBuilder.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"

using namespace llvm;

#define DEBUG_TYPE "scalar-interpolation"

void ScalarInterpolation::generateScalarBlocks(Loop *L, unsigned int SICount) {
  errs() << "before unrolling:\n";
  L->dumpVerbose();
  unrollLoopWithSIFactor(L, SICount);
  errs() << "\nafter unrolling:\n";
  L->dumpVerbose();
  errs() << "\n";
  //  detect original set of blocks (from the beginning to the first latch is
  //  one iteration) todo-si: find a solution for cases in which internal
  //  latches are removed
  errs() << "Number of blocks in the loop: " << L->getNumBlocks() << "\n";
  for (auto BB : L->blocks()) {
    BlocksToVectorize.insert(BB);
    if (L->isLoopExiting(BB) && DT->dominates(BB, L->getLoopLatch()))
      break;
  }
  for (auto BB : L->blocks()) {
    errs() << "Block: " << BB->getName() << ", is vectorized: "
           << ((BlocksToVectorize.find(BB) != BlocksToVectorize.end()) ? "T"
                                                                       : "F")
           << "\n";
  }
}

void ScalarInterpolation::unrollLoopWithSIFactor(Loop *L,
                                                 unsigned int SICount) const {
  LLVM_DEBUG(
      errs()
      << "ScalarInterpolation::generateScalarBlocks is called with SI (debug): "
      << SICount << "\n");
  errs() << "ScalarInterpolation::generateScalarBlocks is called with SI: "
         << SICount << "\n";
  //  generate proper factors for loop unrolling
  UnrollLoopOptions ULO = {/*Count*/ SICount + 1,
                           /*Force*/ false,
                           /*Runtime*/ false,
                           /*AllowExpensiveTripCount*/ false,
                           /*UnrollRemainder*/ false,
                           /*ForgetAllSCEV*/ true};
  //  unroll the loop
  Loop *RemainderLoop = nullptr;
  LoopUnrollResult UnrollResult = UnrollLoop(
      L, ULO, LI, SE, DT, AC, TTI, ORE, /*PreserveLCSSA*/ true, &RemainderLoop);

  assert(UnrollResult != LoopUnrollResult::Unmodified && "Unrolling failed!");
  errs() << "Unrolling was successful!\n";
}

bool ScalarInterpolation::isVectorizable(Instruction *I) {
  return (BlocksToVectorize.find(I->getParent()) != BlocksToVectorize.end());
}

void ScalarInterpolation::initializeSIDataStructures(Loop *OriginalLoop) {
  LastValueMaps = std::vector<ValueToValueMapTy>(SICount);
  Header = OriginalLoop->getHeader();
  LatchBlock = OriginalLoop->getLoopLatch();
  OriginalLoop->getExitBlocks(ExitBlocks);
  for (BasicBlock::iterator I = Header->begin(); isa<PHINode>(I); ++I) {
    OrigPHINode.push_back(cast<PHINode>(I));
  }
  Headers.push_back(Header);
  Latches.push_back(LatchBlock);
  L = OriginalLoop;
}

SmallVector<BasicBlock *>
ScalarInterpolation::createScalarBasicBlocks(BasicBlock *BB) {
  SmallVector<BasicBlock *, 4> NewBBs;
  for (unsigned It = 0; It < SICount; ++It) {
    ValueToValueMapTy VMap;
    BasicBlock *CopiedBB = CloneBasicBlock(BB, VMap, ".si" + Twine(It + 1));

    LastValueMaps[It][BB] = CopiedBB;
    for (ValueToValueMapTy::iterator VI = VMap.begin(); VI != VMap.end();
         ++VI) {
      LastValueMaps[It][VI->first] = VI->second;
    }
    //    todo: try to understand what we should do for the exit blocks and
    //    their phi nodes

    if (BB == Header)
      Headers.push_back(CopiedBB);
    if (BB == LatchBlock)
      Latches.push_back(CopiedBB);

    remapInstructionsInBlocks({CopiedBB}, LastValueMaps[It]);
    NewBBs.push_back(CopiedBB);
    //    todo: fix the incoming value of the header phi nodes
  }
  return NewBBs;
}

// Finds the original instruction of I in iteration Iteration if there is any.
// It checks equality of instructions by comparing their pointers.
const Instruction *ScalarInterpolation::getInstInOriginalBB(unsigned Iteration,
                                                            Instruction *I) {
  //  TODO: change the way you check if two instructions are the same
  for (auto Pair : LastValueMaps[Iteration]) {
    if (Pair.second == I) {
      return dyn_cast<Instruction>(Pair.first);
    }
  }
  return nullptr;
}

#ifndef NDEBUG
void ScalarInterpolation::printValueMap() {
  for (unsigned It = 0; It < SICount; ++It) {
    for (auto pair : LastValueMaps[It]) {
      errs() << "Pair: " << *pair.first << " -> " << *pair.second << "\n";
    }
  }
}
#endif

SmallVector<VPBasicBlock *> ScalarInterpolation::generateVectorBasicBlocks(
    BasicBlock *InputBasicBlock, VPBuilder Builder,
    SmallPtrSetImpl<Instruction *> &DeadInstructions, VPlan &Plan,
    VPRecipeBuilder RecipeBuilder, LoopVectorizationLegality *Legal,
    VFRange &Range) {
  auto NewBBs = createScalarBasicBlocks(InputBasicBlock);
  SmallVector<VPBasicBlock *, 4> VPBBs;
  for (unsigned It = 0; It < SICount; ++It) {
    auto *BB = NewBBs[It];
    VPBasicBlock *VPBB = new VPBasicBlock();
    VPBB->setName(BB->getName());
    Builder.setInsertPoint(VPBB);

    // Introduce each ingredient into VPlan.
    // TODO: Model and preserve debug intrinsics in VPlan.
    for (Instruction &I : BB->instructionsWithoutDebug(false)) {
      Instruction *Instr = &I;
      // First filter out irrelevant instructions, to ensure no recipes are
      // built for them.
      if (isa<BranchInst>(Instr) ||
          DeadInstructions.count(getInstInOriginalBB(It, Instr)))
        continue;

      SmallVector<VPValue *, 4> Operands;
      auto *Phi = dyn_cast<PHINode>(Instr);
      if (Phi && Phi->getParent() == Header) {
        Operands.push_back(Plan.getVPValueOrAddLiveIn(
            Phi->getIncomingValueForBlock(L->getLoopPreheader())));
      } else {
        auto OpRange = Plan.mapToVPValues(Instr->operands());
        Operands = {OpRange.begin(), OpRange.end()};
      }

      // Invariant stores inside loop will be deleted and a single store
      // with the final reduction value will be added to the exit block
      StoreInst *SI;
      if ((SI = dyn_cast<StoreInst>(&I)) &&
          Legal->isInvariantAddressOfReduction(SI->getPointerOperand()))
        continue;
      auto RecipeOrValue = handleReplication(RecipeBuilder, Instr, Range, Plan);
      // If Instr can be simplified to an existing VPValue, use it.
      if (isa<VPValue *>(RecipeOrValue)) {
        auto *VPV = cast<VPValue *>(RecipeOrValue);
        Plan.addVPValue(Instr, VPV);
        // If the re-used value is a recipe, register the recipe for the
        // instruction, in case the recipe for Instr needs to be recorded.
        if (VPRecipeBase *R = VPV->getDefiningRecipe())
          RecipeBuilder.setRecipe(Instr, R);
        continue;
      }
      // Otherwise, add the new recipe.

      VPRecipeBase *Recipe = cast<VPRecipeBase *>(RecipeOrValue);
      errs() << "Instruction: " << *Instr << "\n";
      errs() << "Recipe:\n ";
      Recipe->dump();
      errs() << "\n";
      for (auto *Def : Recipe->definedValues()) {
        auto *UV = Def->getUnderlyingValue();
        errs() << "Value and VPValue in Recipe\n";
        errs() << "Def: " << *Def << "\n";
        errs() << "UV: " << *UV << "\n";
        errs() << "\n";
        Plan.addVPValue(UV, Def);
      }

      RecipeBuilder.setRecipe(Instr, Recipe);
      VPBB->appendRecipe(Recipe);
    }
    VPBBs.push_back(VPBB);
  }
  return VPBBs;
}

Instruction *ScalarInterpolation::getInstInOriginalBB(Instruction *I) {
  const Instruction *OriginalInst = nullptr;
  for (unsigned It = 0; It < SICount; ++It) {
    OriginalInst = getInstInOriginalBB(It, I);
    if (OriginalInst)
      break;
  }
//  errs() << "CurrentInst:  " << *I << "\n";
  if (!OriginalInst)
    return nullptr;
//  errs() << "OriginalInst: " << *OriginalInst << "\n";
  return const_cast<Instruction *>(OriginalInst);
}
