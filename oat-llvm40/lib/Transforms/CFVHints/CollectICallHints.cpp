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
  static FunctionMap *fMap;//函数名与函数ID
  static int FID;

  CollectICallHints() : ModulePass(ID) {}
  /*
    runOnModule: LLVM 模块级优化或分析 Pass 的入口函数（程序执行的起始位置类似于main）
                参数：Module &M：模块
                返回值：bool：判断是否修改
    */
  bool runOnModule(Module &M) override;
  /*
    processFunction:函数初始化工作、遍历M中的间接调用
                参数：Function &F：函数
                返回值：bool:判断是否修改
    */
  bool processFunction(Function &);
  /*
    instrumentICall:插桩函数
              参数：Instruction *I：需要插桩的指令；int fid：函数ID； int count:已经插装函数的数量
              返回值：bool:判断是否修改

    */
  bool instrumentICall(Instruction *I, int fid, int count);
};
} // end of namespace

bool CollectICallHints::runOnModule(Module &M) {
  bool modified = false;
  for (auto &F : M) {//遍历模块M的每一个函数
    //errs() << "runOnModule function name: " << F.getName() << "\n";
    if (F.isDeclaration())//只声明没有实现跳过
      continue;
    if (F.hasFnAttribute(Attribute::OptimizeNone)) /* 函数 F 有 OptimizeNone 属性（表示此函数不进行优化） */
      continue;
    modified |= CollectICallHints::processFunction(F);
  }

  return modified;
}

bool CollectICallHints::processFunction(Function &F) {
  bool modified = false;
  std::string name = F.getName().str();
  int fid;//存储函数ID
  int count = 0;

  //函数是否已经访问过
  if (fMap->find(name) != fMap->end()) {//避免为相同函数重复使用相同的 ID
      (*fMap)[name] = ++FID; 
  }

  fid = (*fMap)[name];

  errs() << "process function: " << F.getName() << "\n";

  for (auto &I : findIndirectCallSites(F)) {//遍历函数F中的所有间接调用
    modified |= instrumentICall(I, fid, count);

    /* label the indirect call cites in this function */
    count++;
  }

  return modified;
}
/*间接调用指令A；      A调用的目标地址：targeFunc；
  插桩的调用指令B；    B调用的目标地址：插入函数 __collect_icall_hints 的地址*/

bool CollectICallHints::instrumentICall(Instruction *I, int fid, int count) {
    /*初始化B的参数 */
    IRBuilder<> B(I);//声明被插桩位置：I
    Module *M = B.GetInsertBlock()->getModule();//获得插桩位置的基本块属于的Modul
    Type *VoidTy = B.getVoidTy();
    Type *I64Ty = B.getInt64Ty();
    ConstantInt *constFid, *constCount;
    Value *targetFunc;//A的目标函数指针
    Value *castVal;//存储将指针类型C转换为整数类型[地址]的值

    errs() << __func__ << " : "<< *I<< "\n";
    errs() << __func__ << " fid : "<< fid << " count: " << count << "\n";

    //判断A的调用类型,并存储A的目标函数指针：targetFunc
    if (auto *callInst = dyn_cast<CallInst>(I)) {//普通调用函数
      targetFunc = callInst->getCalledValue();
    } else if (auto *invokeInst = dyn_cast<InvokeInst>(I)) {//携带异常处理的调用指令
      targetFunc = invokeInst->getCalledValue();
    } else {
      errs() << __func__ << "ERROR: unknown call inst: "<< *I<< "\n";
      return false;
    }

    assert(targetFunc != nullptr);//再次检查目标函数指针是否为空

    /*下面都是初始化插桩的调用指令B，并插桩指令B*/

    //B的目的地址：B调用的插桩函数
    //M->getOrInsertFunction:查找并返回一个指定名称和签名的函数，并返回常量函数
    Constant *ConstCollectICallHints= M->getOrInsertFunction("__collect_icall_hints", //函数名
                                                                    VoidTy,//返回类型
                                                                     I64Ty,//参数
                                                                     I64Ty, //参数
                                                                     I64Ty, //参数
                                                                     nullptr);
    Function *FuncCollectICallHints= cast<Function>(ConstCollectICallHints);//常量指针（Constant*）转换为 Function* 类型的指针
    //参数：
    constFid = ConstantInt::get((IntegerType*)I64Ty, fid);
    constCount = ConstantInt::get((IntegerType*)I64Ty, count);
      //
    castVal = CastInst::Create(Instruction::PtrToInt, //一种类型转换操作(指针类型转换为整数类型)
                                          targetFunc,//待转换的值【目标函数的指针（一个内存地址）】
                                               I64Ty, //64位整数类型
                                          "ptrtoint", //
                                                  I);//插入位置

    //插入函数调用指令B：用于调用函数 FuncCollectICallHints 并传递参数
    B.CreateCall(FuncCollectICallHints/*目的地址：插桩函数*/, 
                {constFid, constCount, castVal}/*{插桩函数参数}*/);

    return true;
}

char CollectICallHints::ID = 0;
int CollectICallHints::FID = 0;
FunctionMap *CollectICallHints::fMap= new FunctionMap();
static RegisterPass<CollectICallHints> X("collect-icall-hints-pass", "Collect Indirect Call Hints Info", false, false);
