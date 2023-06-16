//
// Created by reza on 29/05/23.
//

#ifndef LLVM_SCALARINTERPOLATION_H
#define LLVM_SCALARINTERPOLATION_H

#include "VPlan.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SparseSet.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Vectorize/LoopVectorizationLegality.h"
#include <map>

namespace llvm {

class VPRecipeBuilder;
class VPBuilder;

class ScalarInterpolation {
private:
  unsigned int SICount;
  ScalarEvolution *SE;
  LoopInfo *LI;
  TargetTransformInfo *TTI;
  DominatorTree *DT;
  AssumptionCache *AC;
  OptimizationRemarkEmitter *ORE;
  std::set<BasicBlock *> BlocksToVectorize;
  std::vector<ValueToValueMapTy> LastValueMaps;
  Loop *L;
  BasicBlock *Header;
  BasicBlock *LatchBlock;
  std::vector<BasicBlock *> Headers;
  std::vector<BasicBlock *> Latches;
  std::vector<PHINode *> OrigPHINode;
  SmallVector<BasicBlock *, 4> ExitBlocks;

  const Instruction *getInstInOriginalBB(unsigned int Iteration, Instruction *I);

public:
  ScalarInterpolation(ScalarEvolution *SE, LoopInfo *LI,
                      TargetTransformInfo *TTI, DominatorTree *DT,
                      AssumptionCache *AC, OptimizationRemarkEmitter *ORE)
      : SE(SE), LI(LI), TTI(TTI), DT(DT), AC(AC), ORE(ORE) {
    SICount = 0;
  }
  void setSICount(unsigned int SICount) { this->SICount = SICount; }
  SmallVector<BasicBlock *> createScalarBasicBlocks(BasicBlock *BB);
  void initializeSIDataStructures(Loop *OriginalLoop);
  void generateScalarBlocks(Loop *L, unsigned SICount);
  bool isVectorizable(Instruction *I);
  void unrollLoopWithSIFactor(Loop *L, unsigned int SICount) const;
  SmallVector<VPBasicBlock *>
  generateVectorBasicBlocks(BasicBlock *InputBasicBlock,
                            VPBuilder Builder,
                            SmallPtrSetImpl<Instruction *> &DeadInstructions,
                            VPlan &Plan, VPRecipeBuilder RecipeBuilder,
                            LoopVectorizationLegality *Legal, VFRange &Range);
};

} // namespace llvm
#endif // LLVM_SCALARINTERPOLATION_H
