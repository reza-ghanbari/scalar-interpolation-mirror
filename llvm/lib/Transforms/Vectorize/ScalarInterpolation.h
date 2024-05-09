//
// Created by reza on 18/09/23.
//

#ifndef LLVM_SCALAR_INTERPOLATION_H
#define LLVM_SCALAR_INTERPOLATION_H

#include "LoopVectorizationPlanner.h"

using namespace llvm;

class ScalarInterpolationCostModel {
private:
  SmallVector<VPDef::VPRecipeTy> NonInterpolatableRecipes = {
      llvm::VPInterleaveRecipe::VPInterleaveSC,
      llvm::VPFirstOrderRecurrencePHIRecipe::VPFirstOrderRecurrencePHISC,
      llvm::VPActiveLaneMaskPHIRecipe::VPActiveLaneMaskPHISC,
      llvm::VPWidenSelectRecipe::VPWidenSelectSC,
      llvm::VPPredInstPHIRecipe::VPPredInstPHISC
  };

  DenseMap<const char *, DenseMap<Type *, unsigned>> InstructionTypeMap;

  bool HasReductions = false;

  unsigned NumberOfInstructions = 0;

  unsigned NumberOfComputeInstructions = 0;

  unsigned NumberOfMemoryInstructions = 0;

  float MemToComputeRatio = 0.0;

  void addFeature(Instruction *I, Type *T);

  void extractFeaturesFromLoop(Loop *L);

  unsigned getProfitableSIFactor(Loop* OrigLoop);

  unsigned getValueFromMap(const char* I, Type *T);

public:
  ScalarInterpolationCostModel() {};

  Instruction *getUnderlyingInstructionOfRecipe(VPRecipeBase &R);

  bool hasNonInterpolatableRecipe(VPlan& Plan);

  bool containsNonInterpolatableRecipe(VPlan& Plan);

  unsigned getProfitableSIFactor(VPlan& Plan, Loop* OrigLoop, unsigned UserSI, unsigned MaxSafeElements, bool IsScalarInterpolationEnabled);

  unsigned getSIFactor(VPlan& Plan, Loop* OrigLoop, LoopVectorizationCostModel& CM, std::optional<unsigned int> VScale);

  ElementCount getProfitableVF(VPlan &Plan, LoopVectorizationCostModel& CM, std::optional<unsigned int> VScale);
};



#endif // LLVM_SCALAR_INTERPOLATION_H
