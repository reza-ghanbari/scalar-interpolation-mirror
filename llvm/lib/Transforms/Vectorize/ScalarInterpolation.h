//
// Created by reza on 18/09/23.
//

#ifndef LLVM_SCALAR_INTERPOLATION_H
#define LLVM_SCALAR_INTERPOLATION_H

#include "LoopVectorizationPlanner.h"

using namespace llvm;

class ScalarInterpolationCostModel {
public:
  ScalarInterpolationCostModel() {};

  unsigned getProfitableSIFactor(VPlan& Plan, Loop* OrigLoop, unsigned UserSI);
};



#endif // LLVM_SCALAR_INTERPOLATION_H
