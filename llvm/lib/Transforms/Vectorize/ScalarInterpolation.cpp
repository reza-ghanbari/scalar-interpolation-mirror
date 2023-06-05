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

std::map<BasicBlock *, bool>
ScalarInterpolation::unrollLoop(Loop *L, unsigned int SICount) {
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
  return std::map<BasicBlock *, bool>();
}
