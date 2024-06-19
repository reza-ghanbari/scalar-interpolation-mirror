//
// Created by reza on 18/09/23.
//

#ifndef LLVM_SCALAR_INTERPOLATION_H
#define LLVM_SCALAR_INTERPOLATION_H

#include "LoopVectorizationPlanner.h"
#include "llvm/IR/Instruction.h"
#include <random>

using namespace llvm;

class OperationNode;

class ResourceHandler {
protected:
  float RandomWeight;

  SmallVector<bool> Resources;

  SmallVector<float> Priorities;

  virtual SmallVector<int, 6> getScalarResourcesFor(Instruction& Instr) = 0;

  virtual SmallVector<int, 6> getVectorResourcesFor(Instruction& Instr) = 0;

private:
  SmallVector<int, 6> getResourcesFor(Instruction& Instr, bool isVector);

public:
  ResourceHandler(float RandomWeight): RandomWeight(RandomWeight) {}

  bool isResourceAvailable(int Resource) { return Resources[Resource]; }

  void setResourceUnavailable(int Resource) { Resources[Resource] = false; }

  void setResourceAvailable(int Resource) { Resources[Resource] = true; }

  int scheduleInstructionOnResource(Instruction& Instr, bool isVector);

  bool isResourceAvailableFor(Instruction& Instr, bool isVector);
};

class ResourceHandlerX86: public ResourceHandler {
public:
  ResourceHandlerX86(float RandomWeight): ResourceHandler(RandomWeight) {
    for (int i = 0; i < 7; i++) {
      Resources.push_back(true);
      Priorities.push_back((i / 7));
    }
  }

  virtual SmallVector<int, 6> getScalarResourcesFor(Instruction& Instr) override;

  virtual SmallVector<int, 6> getVectorResourcesFor(Instruction& Instr) override;
};

class ResourceHandlerTSV110: public ResourceHandler {
public:
  ResourceHandlerTSV110(float RandomWeight): ResourceHandler(RandomWeight) {
    for (int i = 0; i < 8; i++) {
      Resources.push_back(true);
      Priorities.push_back(0.5);
    }
    Priorities[0] = 1;
    Priorities[2] = 0.25;
    Priorities[4] = 0.5;
  }

  virtual SmallVector<int, 6> getScalarResourcesFor(Instruction& Instr) override;

  virtual SmallVector<int, 6> getVectorResourcesFor(Instruction& Instr) override;
};

class OperationNode {
private:
  SmallVector<OperationNode*, 6> Predecessors;

  SmallVector<OperationNode*, 6> Successors;

  int StartTime;

  int Duration;

  Instruction* Instr;

  int SIFactor;

  int Priority;

public:
  OperationNode(Instruction* Instr, int StartTime): Instr(Instr) {
    this->StartTime = StartTime;
    this->Duration = -1;
    this->Predecessors = {};
    this->Successors = {};
  }

  OperationNode(OperationNode* Node): StartTime(Node->StartTime), Duration(Node->Duration)
                                       , Instr(Node->Instr), SIFactor(Node->SIFactor), Priority(Node->Priority) {
    this->Predecessors = {};
    this->Successors = {};
  }

  SmallVector<OperationNode*, 6> getPredecessors() { return Predecessors; }

  SmallVector<OperationNode*, 6> getSuccessors() { return Successors; }

  void setStartTime(int StartTime) { this->StartTime = StartTime; }

  bool isScheduled() { return this->Duration != -1; }

  void setDuration(int Duration) { this->Duration = Duration; }

  int getDuration() { return this->Duration; }

  void setDuration(InstructionCost Duration);

  void setSIFactor(int SIFactor) { this->SIFactor = SIFactor; }

  int getSIFactor() { return this->SIFactor; }

  bool isVector() { return this->SIFactor == 0; }

  int getStartTime() { return this->StartTime; }

  int getEndTime() { return this->Duration + this->StartTime; }

  int getPriority() { return this->Priority; }

  void setPriority(int Priority) { this->Priority = Priority; }

  void addPredecessor(OperationNode* Pred) { this->Predecessors.push_back(Pred); }

  void addSuccessor(OperationNode* Succ) { this->Successors.push_back(Succ); }

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

  ResourceHandler* ResHandler;

  int RepeatFactor;

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
  ScalarInterpolationCostModel(LoopVectorizationCostModel& CM, Loop *OrigLoop, std::optional<unsigned int> VScale, int RepeatFactor)
      : CM(CM), OrigLoop(OrigLoop), VScale(VScale), ResHandler(new ResourceHandlerX86(0.5)), RepeatFactor(RepeatFactor) {}

  Instruction *getUnderlyingInstructionOfRecipe(VPRecipeBase &R);

  bool hasNonInterpolatableRecipe(VPlan& Plan);

  bool isLegalToInterpolate(VPlan& Plan);

  bool containsNonInterpolatableRecipe(VPlan& Plan);

  unsigned getProfitableSIFactor(VPlan& Plan, Loop* OrigLoop, unsigned UserSI, unsigned MaxSafeElements, bool IsScalarInterpolationEnabled);

  std::pair<DenseMap<Value*, OperationNode*>, int> getScheduleMap(VPlan &Plan, ElementCount VF, int SIF);

  DenseMap<Value*, OperationNode*> deepCopySchedule(DenseMap<Value*, OperationNode*> Schedule);

  unsigned getSIFactor(VPlan &Plan);

  ElementCount getProfitableVF(VPlan &Plan);

  std::pair<SmallSet<OperationNode*, 30>, int> runListScheduling(SmallVector<DenseMap<Value*, OperationNode*>> schedules, int ScheduleLength);

  std::pair<SmallSet<OperationNode*, 30>, int> repeatListScheduling(SmallVector<DenseMap<Value*, OperationNode*>> schedules, int ScheduleLength);

  SmallSet<OperationNode*, 30> getReadyNodes(SmallVector<DenseMap<Value*, OperationNode*>> schedules);

  void setSIFactorForScheduleMap(DenseMap<Value*, OperationNode*> ScheduleMap, unsigned SIFactor);

  void setPrioritiesForScheduleMap(DenseMap<Value*, OperationNode*> ScheduleMap);

  OperationNode* selectNextNodeToSchedule(SmallSet<OperationNode*, 30> ReadyNodes, int ScheduleLength);
};


#endif // LLVM_SCALAR_INTERPOLATION_H
