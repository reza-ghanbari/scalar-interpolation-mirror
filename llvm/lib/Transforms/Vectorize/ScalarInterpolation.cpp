//
// Created by reza on 29/05/23.
//

#include "ScalarInterpolation.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Dominators.h"

using namespace llvm;

std::map<BasicBlock *, bool>
ScalarInterpolation::unrollLoop(Loop *L, unsigned int Count) {
  return std::map<BasicBlock *, bool>();
}
