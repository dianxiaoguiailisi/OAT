// ==- CollectLoopHints.cpp - 收集控制流验证的循环提示 ------------------===//
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

// 使用 cl::opt 定义了一个命令行选项 collect-loop-hints，可以控制是否启用循环提示收集功能，默认为 true。
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
  // 获取循环的头部基本块并调用 instrumentLoopHeader 在循环头部插入一个函数调用，记录循环的 ID、层级和计数。
  bool runOnLoop(Loop *, int, int, int);

  // 递归地遍历循环及其子循环，对每个循环调用 runOnLoop 进行处理。
  //递归到循环的最底层（最内层的子循环），处理完最底层的循环之后，再逐层返回，处理上一层的循环
  bool runOnLoopAndSubLoops(Loop *L, int fid, int level, int count) {
    bool modified = false;
    int localCount = 0;

    // 遍历当前循环L的素有子循环
    //*L解引用：获得循环对象的子循环列表
    //每次循环，I 都会指向一个子循环
    for (Loop *I : *L) {
      //level + 1 表示子循环的嵌套层级比父循环高 1
      modified |= runOnLoopAndSubLoops(I, fid, level + 1, localCount);
      localCount++;
    }
    //结束子循环遍历
    modified |= runOnLoop(L, fid, level, count);//处理当前循环

    return modified;
  }
//针对每个函数进行操作的 Pass
  bool runOnFunction(Function &F) override {
    LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();//获取当前函数中的循环信息，并将其赋值给 LI
    std::string name = F.getName().str();
    int count = 0;
    int fid;

    // 检查 fMap是否已经包含当前函数名
    if (fMap->find(name) != fMap->end()) {
	        (*fMap)[name] = ++FID; 
    }

    fid = (*fMap)[name];
    // 遍历当前函数中的所有循环（通过 LoopInfo）。对于每一个I，调用 runOnLoopAndSubLoops 递归地遍历该循环和所有子循环，并对每个循环进行处理
    for (Loop *I : *LI) {
      Modified |= runOnLoopAndSubLoops(I, fid, /*initial level*/0, count);
      count++;
    }
    return Modified;
  }
  //表明这个 Pass 需要依赖 LoopInfoWrapperPass 来分析循环信息
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
    // We need to modify IR, so we can't indicate AU to preserve analysis results. 
    // AU.setPreservesAll();
  }
  //插桩函数
  bool instrumentLoopHeader(Instruction *I, int fid, int level, int count);

};
} // end of namespace

bool CollectLoopHints::runOnLoop(Loop *L, int fid, int level, int count) {
  BasicBlock *Header = L->getHeader();//获取当前循环的头部基本块
  Instruction *I = &(*(Header->begin()));//获取循环头部基本块的第一条指令
  bool modified;

  NumOfAffectedLoops++;

  modified = instrumentLoopHeader(I, fid, level, count);//插桩

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
