//
// Created by reza on 18/09/23.
//

#include "ScalarInterpolation.h"
#include "VPlanCFG.h"

#define DEBUG_TYPE "scalar-interpolation"

using namespace llvm;

unsigned ScalarInterpolationCostModel::getProfitableSIFactor(VPlan &Plan, Loop *OrigLoop, unsigned UserSI) {
  auto *VectorLoopRegion = Plan.getVectorLoopRegion();
  errs() << "\n\n\n============================================\n\n\n";
  VectorLoopRegion->dump();
  errs() << "\n\n\n============================================\nEndSI";
  return UserSI;
}
