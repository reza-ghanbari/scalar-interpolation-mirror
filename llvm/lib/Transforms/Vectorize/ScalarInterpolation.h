//
// Created by reza on 29/05/23.
//

#ifndef LLVM_SCALARINTERPOLATION_H
#define LLVM_SCALARINTERPOLATION_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Instructions.h"
#include <map>

namespace llvm {

class ScalarInterpolation {
private:
  unsigned int SICount;
  ScalarEvolution *SE;
  LoopInfo *LI;
  TargetTransformInfo *TTI;
  DominatorTree *DT;
  AssumptionCache *AC;
  OptimizationRemarkEmitter *ORE;

public:
  ScalarInterpolation(ScalarEvolution *SE, LoopInfo *LI,
                      TargetTransformInfo *TTI, DominatorTree *DT,
                      AssumptionCache *AC, OptimizationRemarkEmitter *ORE)
      : SE(SE), LI(LI), TTI(TTI), DT(DT), AC(AC), ORE(ORE) {
    SICount = 0;
  }
  void setSICount(unsigned int SICount) { this->SICount = SICount; }
  std::map<BasicBlock *, bool> unrollLoop(Loop *L, unsigned Count);
};

} // namespace llvm
#endif // LLVM_SCALARINTERPOLATION_H
