```cpp
//===- CollectICallHints.cpp - 收集间接调用提示 ----------------===//
//
//                     LLVM 编译器基础设施
//
// 本文件依据伊利诺伊大学开源许可证发布。详情见 LICENSE.TXT。
//
//===----------------------------------------------------------------------===//
// 本 Pass 与 AArch64 后端的控制流验证 Pass 配合工作。为了提供提示信息，
// 使验证器能够快速验证控制流哈希值，我们需要收集可能导致路径爆炸的控制流事件的提示信息。
// 其中，循环是导致路径爆炸的主要元凶。间接调用也是构建控制流图时一个不确定的部分。
// 本 Pass 将处理间接调用。
//
// TODO：理论上，我们只需要对那些可能有多个或不确定目标函数的间接调用进行插桩。
// 但是，这需要更多的分析来辅助我们的选择。作为初步版本，我们为间接调用提示收集提供了一个框架，
// 在这个框架中，我们将所有间接调用统一处理。
//
// 对于每一个间接调用，我们记录以下信息：
// 三元组 <母函数 ID，可能的目标函数 ID>。
//   母函数 ID：包含间接调用的函数。
//   icall-cite-count：母函数中该间接调用指令的标签。
//   target-function-addr：在运行时实际的目标函数地址。
```

#include "llvm/Pass.h"
#include <string>

#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/IndirectCallSiteVisitor.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "collect-icall-hints"

using namespace llvm;

typedef std::map<std::string, int> FunctionMap;

namespace {
struct  CollectICallHints : public ModulePass {
  static char ID;
  static FunctionMap *fMap;
  static int FID;

  CollectICallHints() : ModulePass(ID) {}
  bool runOnModule(Module &M) override;
  bool processFunction(Function &);
  bool instrumentICall(Instruction *I, int fid, int count);
};
} // end of namespace

bool CollectICallHints::runOnModule(Module &M) {
  bool modified = false;
  for (auto &F : M) {
    //errs() << "runOnModule function name: " << F.getName() << "\n";
    if (F.isDeclaration())
      continue;
    if (F.hasFnAttribute(Attribute::OptimizeNone)) /* TODO:need further check */
      continue;
    modified |= CollectICallHints::processFunction(F);
  }

  return modified;
}

bool CollectICallHints::processFunction(Function &F) {
  bool modified = false;
  std::string name = F.getName().str();
  int fid;
  int count = 0;

  // check whether we have visit this function before
  if (fMap->find(name) != fMap->end()) {
      (*fMap)[name] = ++FID; 
  }

  fid = (*fMap)[name];

  errs() << "process function: " << F.getName() << "\n";

  for (auto &I : findIndirectCallSites(F)) {
    modified |= instrumentICall(I, fid, count);

    /* label the indirect call cites in this function */
    count++;
  }

  return modified;
}

bool CollectICallHints::instrumentICall(Instruction *I, int fid, int count) {
    IRBuilder<> B(I);
    Module *M = B.GetInsertBlock()->getModule();
    Type *VoidTy = B.getVoidTy();
    Type *I64Ty = B.getInt64Ty();
    ConstantInt *constFid, *constCount;
    Value *targetFunc;
    Value *castVal;

    errs() << __func__ << " : "<< *I<< "\n";
    errs() << __func__ << " fid : "<< fid << " count: " << count << "\n";

    if (auto *callInst = dyn_cast<CallInst>(I)) {
      targetFunc = callInst->getCalledValue();
    } else if (auto *invokeInst = dyn_cast<InvokeInst>(I)) {
      targetFunc = invokeInst->getCalledValue();
    } else {
      errs() << __func__ << "ERROR: unknown call inst: "<< *I<< "\n";
      return false;
    }

    assert(targetFunc != nullptr);

    Constant *ConstCollectICallHints= M->getOrInsertFunction("__collect_icall_hints", VoidTy,
                                                                     I64Ty,
                                                                     I64Ty, 
                                                                     I64Ty, 
                                                                     nullptr);

    Function *FuncCollectICallHints= cast<Function>(ConstCollectICallHints);
    constFid = ConstantInt::get((IntegerType*)I64Ty, fid);
    constCount = ConstantInt::get((IntegerType*)I64Ty, count);
    castVal = CastInst::Create(Instruction::PtrToInt, targetFunc, I64Ty, "ptrtoint", I);

    B.CreateCall(FuncCollectICallHints, {constFid, constCount, castVal});

    return true;
}

char CollectICallHints::ID = 0;
int CollectICallHints::FID = 0;
FunctionMap *CollectICallHints::fMap= new FunctionMap();
static RegisterPass<CollectICallHints> X("collect-icall-hints-pass", "Collect Indirect Call Hints Info", false, false);
