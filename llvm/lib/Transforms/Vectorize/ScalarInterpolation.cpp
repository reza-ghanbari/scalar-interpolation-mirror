//
// Created by reza on 29/05/23.
//

#include "ScalarInterpolation.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"

using namespace llvm;

#define DEBUG_TYPE "scalar-interpolation"

void ScalarInterpolation::unrollLoop(Loop *L, unsigned int SICount) {
  LLVM_DEBUG(
      dbgs() << "ScalarInterpolation::unrollLoop is called with SI (debug): "
             << SICount << "\n");
  errs() << "ScalarInterpolation::unrollLoop is called with SI: " << SICount
         << "\n";
  //  generate proper factors for loop unrolling

  //  unroll the loop
  Loop *RemainderLoop = nullptr;
  LoopUnrollResult UnrollResult =
      UnrollLoop(L, {SICount + 1, false, false, false, false, true}, LI, SE,
                 DT, AC, TTI, ORE, /*PreserveLCSSA*/true, &RemainderLoop);

  assert(UnrollResult != LoopUnrollResult::Unmodified && "Unrolling failed!");
  errs() << "Unrolling was successful!\n";
//  detect original set of blocks (from the beginning to the first latch is one iteration)
//  todo-si: find a solution for cases in which internal latches are removed
  for (auto BB: L->blocks()) {
    BlocksToVectorize.insert(BB);
    if (L->isLoopExiting(BB) && DT->dominates(BB, L->getLoopLatch()))
      break;
  }
}
bool ScalarInterpolation::isVectorizable(Instruction *I) {
  return (BlocksToVectorize.find(I->getParent()) != BlocksToVectorize.end());
}
