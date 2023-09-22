//
// Created by reza on 18/09/23.
//

#include "ScalarInterpolation.h"
#include "VPlanCFG.h"

#define DEBUG_TYPE "scalar-interpolation"

using namespace llvm;

VPRecipeBase *CloneRecipe(VPRecipeBase *R) {
  switch (R->getVPDefID()) {
  case VPDef::VPWidenMemoryInstructionSC:
    VPWidenMemoryInstructionRecipe *Converted =
        dyn_cast<VPWidenMemoryInstructionRecipe>(R);
    if (Converted->isStore())
      return new VPWidenMemoryInstructionRecipe(
          dyn_cast<StoreInst>(Converted->getIngredient()), Converted->getOperand(0),
          Converted->getOperand(1), Converted->getMask(),
          Converted->isConsecutive(), Converted->isReverse());
    return new VPWidenMemoryInstructionRecipe(
        dyn_cast<LoadInst>(Converted->getIngredient()), Converted->getOperand(0), Converted->getMask(),
        Converted->isConsecutive(), Converted->isReverse());
//  TODO-SI: finish the Recipe copying here!
  }
}

VPRegionBlock *
ScalarInterpolationCostModel::copyOriginalVPlan(llvm::VPlan &Original,
                                                Loop *OrigLoop) {
  errs() << "SI: Here is the VPlan Blocks:\n";
  auto *VectorLoopRegion = Original.getVectorLoopRegion();
  auto *CopiedLoopRegion = new VPRegionBlock();
  CopiedLoopRegion->setName("copied loop region");
  ReversePostOrderTraversal<VPBlockDeepTraversalWrapper<VPBlockBase *>> RPOT(
      VectorLoopRegion->getEntry());
  for (auto *VPBB : reverse(VPBlockUtils::blocksOnly<VPBasicBlock>(RPOT))) {
    //    VPBB->getRecipeList().cloneFrom()
    for (auto &R : *VPBB) {
//      todo-si: do cloning of the recipe and create the VPBB based on that
    }
  }
}

unsigned ScalarInterpolationCostModel::getProfitableScalarInterpolationFactor(
    VPlan &Plan, Loop *OrigLoop) {
  auto *VectorLoopRegion = Plan.getVectorLoopRegion();
  auto *CopiedLoopRegion = this->copyOriginalVPlan(Plan, OrigLoop);
  ReversePostOrderTraversal<VPBlockDeepTraversalWrapper<VPBlockBase *>> RPOT(
      CopiedLoopRegion->getEntry());
  for (auto *VPBB : reverse(VPBlockUtils::blocksOnly<VPBasicBlock>(RPOT)))
    VPlanTransforms::applyInterpolationOnVPBasicBlock(VPBB, 1, Plan, OrigLoop);
  Plan.clearInterpolatedValue2VPValue();
  errs() << "\n\n\n============================================\n\n\n";
  VectorLoopRegion->dump();
  errs() << "\n\n\n============================================\nEndSI";
  errs() << "\n\n\n============================================\n\n\n";
  CopiedLoopRegion->dump();
  errs() << "\n\n\n============================================\nEndSI";
  return 0;
}
