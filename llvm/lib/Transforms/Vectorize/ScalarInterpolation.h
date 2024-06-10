//
// Created by reza on 18/09/23.
//

#ifndef LLVM_SCALAR_INTERPOLATION_H
#define LLVM_SCALAR_INTERPOLATION_H

#include "LoopVectorizationPlanner.h"
#include "llvm/IR/Instruction.h"

using namespace llvm;

class OperationNode;

class OperationNode {
private:
  SmallVector<OperationNode*, 6> Predecessors;

  std::pair<int, int> Duration;

  Instruction* Instr;

public:
  OperationNode(Instruction* Instr, int StartTime): Instr(Instr) {
    this->Duration = { StartTime, -1 };
    this->Predecessors = {};
  }

  SmallVector<OperationNode*, 6> getPredecessors() { return Predecessors; }

  void setStartTime(int StartTime) { this->Duration.first = StartTime; }

  bool isScheduled() { return this->Duration.second != -1; }

  void setDuration(int Duration) { this->Duration.second = Duration + this->Duration.first; }

  void setDuration(InstructionCost Duration);

  int getStartTime() { return this->Duration.first; }

  int getEndTime() { return this->Duration.second; }

  void addPredecessor(OperationNode* Pred) { this->Predecessors.push_back(Pred); }

  Instruction* getInstruction() { return this->Instr; }

  void print(raw_fd_ostream &OS);
};

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

  DenseMap<Value*, OperationNode*> initScheduleMap(VPlan &Plan, ElementCount VF);

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

  OperationNode* getScheduleOf(VPRecipeBase& R, DenseMap<Value*, OperationNode*> ReadyValues);

public:
  ScalarInterpolationCostModel(LoopVectorizationCostModel& CM, Loop *OrigLoop, std::optional<unsigned int> VScale)
      : CM(CM), OrigLoop(OrigLoop), VScale(VScale) {};

  Instruction *getUnderlyingInstructionOfRecipe(VPRecipeBase &R);

  bool hasNonInterpolatableRecipe(VPlan& Plan);

  bool containsNonInterpolatableRecipe(VPlan& Plan);

  unsigned getProfitableSIFactor(VPlan& Plan, Loop* OrigLoop, unsigned UserSI, unsigned MaxSafeElements, bool IsScalarInterpolationEnabled);

  DenseMap<Value*, OperationNode*> getScheduleMap(VPlan &Plan, ElementCount VF);

  unsigned getSIFactor(VPlan &Plan);

  ElementCount getProfitableVF(VPlan &Plan);

  SmallVector<int, 6> getResourcesFor(Instruction& Instr, bool isVector);

  SmallVector<int, 6> getScalarResourcesFor(Instruction& Instr);

  SmallVector<int, 6> getVectorResourcesFor(Instruction& Instr);

  DenseMap<Value*, OperationNode*> applyListScheduling(DenseMap<Value*, OperationNode*> schedule);
};


#endif // LLVM_SCALAR_INTERPOLATION_H
