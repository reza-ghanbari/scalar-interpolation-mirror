//
// Created by reza on 18/09/23.
//

#include "ScalarInterpolation.h"
#include "VPlanCFG.h"

#define DEBUG_TYPE "scalar-interpolation"

using namespace llvm;

bool ScalarInterpolationCostModel::hasInterleavingGroups(llvm::VPlan &Plan) {
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

unsigned ScalarInterpolationCostModel::getProfitableSIFactor(VPlan &Plan, Loop *OrigLoop, unsigned UserSI) {
  auto *VectorLoopRegion = Plan.getVectorLoopRegion();
  if (hasInterleavingGroups(Plan)) {
    return 0;
  }
  errs() << "\n\n\n============================================\n\n\n";
  VectorLoopRegion->dump();
  errs() << "\n\n\n============================================\nEndSI";
  return UserSI;
}
