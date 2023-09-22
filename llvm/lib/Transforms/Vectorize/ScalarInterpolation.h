//
// Created by reza on 18/09/23.
//

#ifndef LLVM_SCALARINTERPOLATION_H
#define LLVM_SCALARINTERPOLATION_H

#include "LoopVectorizationPlanner.h"
#include "VPlanTransforms.h"

using namespace llvm;

class ScalarInterpolationCostModel {
private:
  VPRegionBlock* copyOriginalVPlan(VPlan& Original, Loop* OrigLoop);

public:
  ScalarInterpolationCostModel() {};

  unsigned getProfitableScalarInterpolationFactor(VPlan& Plan, Loop* OrigLoop);
};



#endif // LLVM_SCALARINTERPOLATION_H
