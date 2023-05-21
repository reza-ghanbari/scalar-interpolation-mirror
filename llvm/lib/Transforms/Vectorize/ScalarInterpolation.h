//
// Created by reza on 18/05/23.
//

#ifndef LLVM_PROJECT_SCALAR_INTERPOLATION_H
#define LLVM_PROJECT_SCALAR_INTERPOLATION_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "LoopVectorizationPlanner.h"

using namespace llvm;

class VPlanExplorer {
  private:
    SmallVector<VectorizationFactor, 8> ProfitableVFs;

  public:
    VPlanExplorer() {};
    void setProfitableVFs(SmallVector<VectorizationFactor, 8> ProfitableVFs);
};

#endif // LLVM_PROJECT_SCALAR_INTERPOLATION_H
