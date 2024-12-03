//===- CollectIBranchHints.cpp - 收集间接分支提示 ------------===//
//
//                     LLVM 编译器基础设施
//
// 本文件按照伊利诺伊大学开放源代码许可证分发。详细信息请参阅 LICENSE.TXT 文件。
//
//===----------------------------------------------------------------------===//
// 该 pass 与 AArch64 后端的控制流验证 pass 协作。
// 为了为验证器提供快速验证控制流哈希值的提示信息，我们需要收集那些可能导致路径爆炸的控制流事件的提示信息。
// 其中，循环是路径爆炸的主要原因。间接调用/分支也是构建控制流图时的不确定部分。
// 我们将在这个 pass 中处理间接分支。
//
// TODO：理论上，我们只需要对那些具有许多或不确定目标地址的间接分支进行插装。
// 但是，这需要更多的分析来辅助选择。作为初步版本，我们为间接分支提示收集提供了一个框架，
// 其中我们将所有间接分支统一处理。
//
// 对于每个间接分支，我们记录以下信息：
// 三元组 <母函数ID, 可能目标ID>。
//   母函数ID：包含间接函数调用的函数。
//   ibranch-cite-count：
//   目标地址：
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include <string>
#include "llvm/Analysis/CFG.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"

//宏定义
#define DEBUG_TYPE "collect-ibranch-hints"
using namespace llvm;
typedef std::map<std::string, int> FunctionMap;

namespace {
  struct  CollectIBranchHints : public FunctionPass {
    static char ID;
    static FunctionMap *fMap;//存储了函数名到函数 ID 的映射
    static int FID;

    CollectIBranchHints() : FunctionPass(ID) {}
    bool runOnFunction(Function &F) override;
    bool instrumentIBranch(Instruction *I, int fid, int count);
  };
} 

bool CollectIBranchHints::runOnFunction(Function &F) {
  bool modified = false;
  std::string name = F.getName().str();
  int fid;
  int count = 0;

  // 判断是否插桩过该函数
  if (fMap->find(name) != fMap->end()) {
      (*fMap)[name] = ++FID; //每次遇到一个新的函数，赋予新的ID
  }
  fid = (*fMap)[name];

  //遍历每一个基本块
  for (auto &BB : F) {
    /*
      BB.getTerminator():获得每一个基本块的终止指令（最后一条指令【控制流指令】）
      dyn_cast<A>(B)将B转换为A类型，若成功返回该指针；失败返回nullptr
      按位或赋值：A |= B 等价于 A = A | B
      */
    if (IndirectBrInst *IBI = dyn_cast<IndirectBrInst>(BB.getTerminator())) {//若是间接指令
      modified |= instrumentIBranch(IBI, fid, count);//进行插桩，IBI：间接指令;fid:函数ID；count:插桩数量
      /* 标记该函数中的间接分支引用*/
      count++;
    }
  }
  return modified;
}
bool CollectIBranchHints::instrumentIBranch(Instruction *I, int fid, int count) {
    IRBuilder<> B(I);//指明插入指令的位置（I之后）
    Module *M = B.GetInsertBlock()->getModule();//获得当前模块M
    Type *VoidTy = B.getVoidTy();//获得void类型
    Type *I64Ty = B.getInt64Ty();//获得int类型
    ConstantInt *constFid, *constCount;//函数ID，分支计数
    Value *target;//存储间接分支指令的目标地址指针
    Value *castVal;// 存储转换【整数类型（i64 类型）】的目标地址，传递给需要整数参数的函数

    errs() << __func__ << " : "<< *I<< "\n";
    errs() << __func__ << " fid : "<< fid << " count: " << count << "\n";
    //判断是否是间接指令
    if (auto *indirectBranchInst = dyn_cast<IndirectBrInst>(I)) {
      target = indirectBranchInst->getAddress();
    } else {//不是直接返回false
      errs() << __func__ << "ERROR: unknown indirect branch inst: "<< *I<< "\n";
      return false;
    }
    //再次确保目标指令指针不为空
    assert(target != nullptr);

    //在模块中插入函数声明
    Constant *ConstCollectIBranchHints= M->getOrInsertFunction("__collect_ibranch_hints"/*外部函数名*/,VoidTy/*返回类型*/,I64Ty/*参数*/, I64Ty/*参数*/, nullptr/*参数结束标志*/);
    Function *FuncCollectIBranchHints= cast<Function>(ConstCollectIBranchHints);//将之前插入的外部函数声明转换为 Function 类型指针
    
    //参数定义
      constFid = ConstantInt::get((IntegerType*)I64Ty, fid);
      constCount = ConstantInt::get((IntegerType*)I64Ty, count);
      //将指针类型转化为整数类型（无法直接进行必须这样操作）
      castVal = CastInst::Create(Instruction::PtrToInt, target, I64Ty, "ptrtoint", I);
    //插入调用指令
    B.CreateCall(FuncCollectIBranchHints, {constFid, constCount, castVal});
    return true;
}
//静态成员变量初始化
char CollectIBranchHints::ID = 0;
int CollectIBranchHints::FID = 0;
FunctionMap *CollectIBranchHints::fMap= new FunctionMap();
//注册pass
static RegisterPass<CollectIBranchHints> X("collect-ibranch-hints-pass", "Collect Indirect Branch Hints Info", false, false);
