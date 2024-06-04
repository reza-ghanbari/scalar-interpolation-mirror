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

  LoopVectorizationCostModel& CM;

  Loop *OrigLoop;

  std::optional<unsigned int> VScale;

  DenseMap<Value*, std::pair<int, int>> initScheduleMap(VPlan &Plan);

  ElementCount VF;

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

  std::optional<int> getScheduleOf(VPRecipeBase& R, DenseMap<Value*, std::pair<int, int>> ReadyValues);

public:
  ScalarInterpolationCostModel(LoopVectorizationCostModel& CM, Loop *OrigLoop, std::optional<unsigned int> VScale)
      : CM(CM), OrigLoop(OrigLoop), VScale(VScale) {};

  Instruction *getUnderlyingInstructionOfRecipe(VPRecipeBase &R);

  bool hasNonInterpolatableRecipe(VPlan& Plan);

  bool containsNonInterpolatableRecipe(VPlan& Plan);

  unsigned getProfitableSIFactor(VPlan& Plan, Loop* OrigLoop, unsigned UserSI, unsigned MaxSafeElements, bool IsScalarInterpolationEnabled);

  unsigned getSIFactor(VPlan &Plan);

  ElementCount getProfitableVF(VPlan &Plan);
};



#endif // LLVM_SCALAR_INTERPOLATION_H
