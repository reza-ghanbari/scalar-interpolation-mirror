//
// Created by reza on 18/09/23.
//

#include "ScalarInterpolation.h"
#include "VPlanCFG.h"

#define DEBUG_TYPE "scalar-interpolation"

using namespace llvm;

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

unsigned ScalarInterpolationCostModel::getProfitableSIFactor(VPlan &Plan, Loop *OrigLoop, unsigned UserSI) {
  if (hasNonInterpolatableRecipe(Plan)) {
    return 0;
  }
//  TODO: develop the cost model here.
  return UserSI;
}
