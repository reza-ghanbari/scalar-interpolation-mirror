//
// Created by reza on 18/05/23.
//
#include "ScalarInterpolation.h"

#define DEBUG_TYPE "scalar-interpolation"

using namespace llvm;

void VPlanExplorer::setProfitableVFs(SmallVector<VectorizationFactor, 8> VFs) {
  LLVM_DEBUG(dbgs() << "VPlanExplorer::setProfitableVFs\n");
  this->ProfitableVFs = VFs;
  for (auto VF : this->ProfitableVFs) {
    LLVM_DEBUG(dbgs() << "VF: " << VF.Width << "\n");
  }
}
