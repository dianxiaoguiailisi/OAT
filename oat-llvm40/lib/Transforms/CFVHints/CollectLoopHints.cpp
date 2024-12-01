```cpp
//==- CollectLoopHints.cpp - 收集控制流验证的循环提示 ------------------===//
//
//                     LLVM 编译器基础设施
//
// 本文件依据伊利诺伊大学开源许可证发布。详情见 LICENSE.TXT。
//
//===----------------------------------------------------------------------===//
// 本 Pass 与 AArch64 后端的控制流验证 Pass 配合工作。为了提供提示信息，
// 使验证器能够快速验证控制流哈希值，我们需要收集可能导致路径爆炸的控制流事件的提示信息。
// 其中，循环是导致路径爆炸的主要元凶。间接调用也是构建控制流图时一个不确定的部分。
// 本 Pass 将处理间接调用，在其他 Pass 中处理。
//
// TODO：理论上，我们只需要对那些体内含有间接跳转或间接调用的循环进行插桩。
// 然而，循环内的函数调用和跳转指令可能使得选择变得困难，因此这个初步版本将对所有循环进行插桩。
//
// 对于函数中的每个循环，将会分配一个标签，标签为三元组 <函数名，循环计数，循环层级>。
//   函数名：我们可能会收集函数名信息，并为其分配一个 ID。
//   循环计数：对于相同层级的循环，分配一个计数值。
//   循环层级：循环可能是嵌套的，因此需要一个层级编号来区分不同层级的循环。
```

#include "llvm/Pass.h"
#include <string>

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Mangler.h" 
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "collect-loop-hints"

STATISTIC(NumOfAffectedLoops, "Number of entry safepoints inserted");

using namespace llvm;

typedef std::map<std::string, int> FunctionMap;

// Make it configurable whether or not to instrument the loop.
static cl::opt<bool> CollectLoopHintsInfo("collect-loop-hints", cl::Hidden,
                                  cl::init(true));

namespace {
struct  CollectLoopHints : public FunctionPass {
  static char ID;
  static FunctionMap *fMap;
  static int FID;

  bool Modified = false;
  LoopInfo *LI = nullptr;

  CollectLoopHints() : FunctionPass(ID) {};

  bool runOnLoop(Loop *, int, int, int);
  bool runOnLoopAndSubLoops(Loop *L, int fid, int level, int count) {
    bool modified = false;
    int localCount = 0;

    // Visit all the subloops
    for (Loop *I : *L) {
      modified |= runOnLoopAndSubLoops(I, fid, level + 1, localCount);
      localCount++;
    }
    modified |= runOnLoop(L, fid, level, count);

    return modified;
  }

  bool runOnFunction(Function &F) override {
    LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    std::string name = F.getName().str();
    int count = 0;
    int fid;

    // check whether we have visit this function before
    if (fMap->find(name) != fMap->end()) {
	(*fMap)[name] = ++FID; 
    }

    fid = (*fMap)[name];

    for (Loop *I : *LI) {
      Modified |= runOnLoopAndSubLoops(I, fid, /*initial level*/0, count);
      count++;
    }
    return Modified;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
    // We need to modify IR, so we can't indicate AU to preserve analysis results. 
    // AU.setPreservesAll();
  }

  bool instrumentLoopHeader(Instruction *I, int fid, int level, int count);

};
} // end of namespace

bool CollectLoopHints::runOnLoop(Loop *L, int fid, int level, int count) {
  // TODO: do instrumentation later
  BasicBlock *Header = L->getHeader();
  Instruction *I = &(*(Header->begin()));
  bool modified;

  NumOfAffectedLoops++;

  modified = instrumentLoopHeader(I, fid, level, count);

  errs() << __func__ << "function id: " << fid << " level: " << level << " count: " << count << "\n";
  errs() << "header : " << Header->getName() << "\n";

  return modified;
}

bool CollectLoopHints::instrumentLoopHeader(Instruction *I, int fid, int level, int count) {
    IRBuilder<> B(I);
    Module *M = B.GetInsertBlock()->getModule();
    Type *VoidTy = B.getVoidTy();
    Type *I32Ty = B.getInt32Ty();
    ConstantInt *cfid, *clevel, *ccount;

    errs() << __func__ << " : "<< *I<< "\n";

    Constant *ConstCollectLoopHints= M->getOrInsertFunction("__collect_loop_hints", VoidTy,
                                                                     I32Ty,
                                                                     I32Ty, 
                                                                     I32Ty, 
                                                                     nullptr);

    Function *FuncCollectLoopHints= cast<Function>(ConstCollectLoopHints);
    cfid = ConstantInt::get((IntegerType*)I32Ty, fid);
    clevel = ConstantInt::get((IntegerType*)I32Ty, level);
    ccount = ConstantInt::get((IntegerType*)I32Ty, count);

    B.CreateCall(FuncCollectLoopHints, {cfid, clevel, ccount});

    return true;
}

char CollectLoopHints::ID = 0;
int CollectLoopHints::FID = 0;
FunctionMap *CollectLoopHints::fMap= new FunctionMap();
static RegisterPass<CollectLoopHints> X("collect-loop-hints-pass", "Collect Loop Hints Info", false, false);
