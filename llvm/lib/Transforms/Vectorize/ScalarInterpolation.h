//
// Created by reza on 18/09/23.
//

#ifndef LLVM_SCALAR_INTERPOLATION_H
#define LLVM_SCALAR_INTERPOLATION_H

#include "LoopVectorizationPlanner.h"
#include "llvm/IR/Instruction.h"
#include <random>

#define LV_NAME "loop-vectorize"
#define DEBUG_TYPE LV_NAME

using namespace llvm;

class OperationNode;

class ResourceHandler {
protected:
  SmallVector<bool> Resources;

  SmallVector<float> Priorities;

  void initialize(SmallVector<SmallVector<int>> PortUniqueness);

  virtual SmallVector<int, 6> getScalarResourcesFor(Instruction& Instr) = 0;

  virtual SmallVector<int, 6> getVectorResourcesFor(Instruction& Instr) = 0;

private:
  SmallVector<int, 6> getResourcesFor(Instruction& Instr, bool isVector);

public:
  ResourceHandler() {}

  bool hasResourceFor(Instruction& Instr, bool isVector) { return getResourcesFor(Instr, isVector).size() > 0; }

  bool isResourceAvailable(int Resource) { return Resources[Resource] || Resource == -1; }

  void setResourceUnavailable(int Resource) { if (Resource != -1) Resources[Resource] = false; }

  void setResourceAvailable(int Resource) { if (Resource != -1) Resources[Resource] = true; }

  int scheduleInstructionOnResource(Instruction& Instr, bool isVector, float RandomWeight);

  bool isResourceAvailableFor(Instruction& Instr, bool isVector);
};

class ResourceHandlerX86: public ResourceHandler {
public:
  ResourceHandlerX86(): ResourceHandler() {
    this->initialize({
        {4, 1, 3, 2, 1, 2},
        {4, 1, 3, 2, 1},
        {4, 1, 3, 1},
        {4, 2},
        {3, 2},
        {3, 2},
        {1},
        {3}
    });
  }

  virtual SmallVector<int, 6> getScalarResourcesFor(Instruction& Instr) override;

  virtual SmallVector<int, 6> getVectorResourcesFor(Instruction& Instr) override;
};

class ResourceHandlerTSV110: public ResourceHandler {
public:
  ResourceHandlerTSV110(): ResourceHandler() {
    this->initialize({
        {3},
        {2, 1, 3},
        {2, 3},
        {1, 1},
        {2, 1, 1},
        {2},
        {2, 2},
        {2, 2}
    });
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

  unsigned int IC;

  ResourceHandler* ResHandler;

  int Budget;

  int StabilityLimit;

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

  bool hasFloatingPointInstruction(VPBasicBlock *VPBB);

  bool hasInstructionWithUnknownResource();

  bool isInWorkingList(Instruction* Instr, SmallVector<OperationNode*> WorkingList);

public:
  ScalarInterpolationCostModel(LoopVectorizationCostModel& CM, Loop *OrigLoop, std::optional<unsigned int> VScale, int Budget, int StabilityLimit)
      : CM(CM), OrigLoop(OrigLoop), VScale(VScale), ResHandler(new ResourceHandlerX86()), Budget(Budget), StabilityLimit(StabilityLimit) {}

  Instruction *getUnderlyingInstructionOfRecipe(VPRecipeBase &R);

  bool hasNonInterpolatableRecipe(VPlan& Plan);

  bool isLegalToInterpolate(VPlan& Plan);

  bool containsNonInterpolatableRecipe(VPlan& Plan);

  unsigned getProfitableSIFactor(VPlan& Plan, unsigned int UserIC, unsigned UserSI, unsigned MaxSafeElements, bool IsScalarInterpolationEnabled);

  std::pair<DenseMap<Value*, OperationNode*>, int> getScheduleMap(VPlan &Plan, ElementCount VF, int SIF);

  DenseMap<Value*, OperationNode*> deepCopySchedule(DenseMap<Value*, OperationNode*> Schedule);

  unsigned getSIFactor(VPlan &Plan, unsigned int UserIC);

  int getProfitableIC(unsigned UserIC);

  ElementCount getProfitableVF(VPlan &Plan);

  std::pair<SmallSet<OperationNode*, 30>, int> runListScheduling(SmallVector<DenseMap<Value*, OperationNode*>> schedules, int ScheduleLength, float RandomWeight);

  std::pair<SmallSet<OperationNode*, 30>, int> repeatListScheduling(SmallVector<DenseMap<Value*, OperationNode*>> schedules, int ScheduleLength);

  SmallSet<OperationNode*, 30> getReadyNodes(SmallVector<DenseMap<Value*, OperationNode*>> schedules);

  void setSIFactorForScheduleMap(DenseMap<Value*, OperationNode*> ScheduleMap, unsigned SIFactor);

  void setPrioritiesForScheduleMap(DenseMap<Value*, OperationNode*> ScheduleMap);

  bool isResourcesAvailableForScheduleMap(DenseMap<llvm::Value *, OperationNode *> ScheduleMap);

  OperationNode* selectNextNodeToSchedule(SmallSet<OperationNode*, 30> ReadyNodes, int ScheduleLength);
};


#endif // LLVM_SCALAR_INTERPOLATION_H
