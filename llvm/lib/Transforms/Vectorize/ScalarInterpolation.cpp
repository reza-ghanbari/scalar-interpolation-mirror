//
// Created by reza on 29/05/23.
//

#include "ScalarInterpolation.h"
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

void ScalarInterpolation::initializeSIDataStructures(Loop *L) {
  LastValueMaps = std::vector<ValueToValueMapTy>(SICount);
  Header = L->getHeader();
  LatchBlock = L->getLoopLatch();
  L->getExitBlocks(ExitBlocks);
  for (BasicBlock::iterator I = Header->begin(); isa<PHINode>(I); ++I) {
    OrigPHINode.push_back(cast<PHINode>(I));
  }
  Headers.push_back(Header);
  Latches.push_back(LatchBlock);
}

VPBasicBlock *ScalarInterpolation::createVectorBlock(BasicBlock *BB) {
  for (int It = 0; It < SICount; ++It) {
    ValueToValueMapTy VMap;
    BasicBlock *copiedBB = CloneBasicBlock(BB, VMap, ".si" + Twine(It + 1));

    if (BB == Header)
      for (PHINode *OrigPHI : OrigPHINode) {
        PHINode *NewPHI = cast<PHINode>(VMap[OrigPHI]);
        VMap[OrigPHI] = NewPHI->getIncomingValueForBlock(LatchBlock);
        NewPHI->eraseFromParent();
      }
    LastValueMaps[It][BB] = copiedBB;
    for (ValueToValueMapTy::iterator VI = VMap.begin(); VI != VMap.end(); ++VI)
      LastValueMaps[It][VI->first] = VI->second;

    remapInstructionsInBlocks({copiedBB}, LastValueMaps[It]);
  }
  return nullptr;
}
