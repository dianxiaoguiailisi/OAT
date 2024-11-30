//==- CollectCondBranchHints.cpp - 为控制流验证收集条件分支提示 ------------------===//
//
//                     LLVM 编译器基础设施
//
// 本文件根据伊利诺伊大学开源许可协议发布。详情请参见 LICENSE.TXT。
//
//===----------------------------------------------------------------------===//
// 该 Pass 与 AArch64 后端的控制流验证（Control-Flow Verification） Pass 协作。
// 为了提供提示信息，使得验证器能够快速验证控制流的哈希值，我们需要为
// 每个条件分支收集提示信息，使用 1 位来记录基本的“已采取”或“未采取”信息。
//===----------------------------------------------------------------------===//


#include "llvm/Pass.h"
#include <string>
#include <vector>
#include <fstream>

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

#define DEBUG_TYPE "collect-cond-branch-hints"

// “下面定义的宏仅用于评估，控制是否对所有函数进行插桩。”
#define INSTRUMENT_ALL

STATISTIC(NumOfCondBranches, "Number of conditional branch inst");//统计 被插桩的条件分支指令的数量，并在最终的编译输出中显示这个统计信息

using namespace llvm;

typedef std::map<std::string, int> FunctionMap;

//用于控制是否收集条件分支提示。默认值是 true，可以通过命令行设置为 false 来禁用此功能。
static cl::opt<bool> CollectCondBranchHintsInfo("collect-cond-branch-hints", cl::Hidden,
                                  cl::init(true));
//指定一个文件，文件中列出了需要插桩的函数名称
static cl::opt<std::string> InstrumentFunctionNameListFile("funclist", cl::Hidden, cl::desc("Specify input file that list the function names that need to be instrumented"),
                                  cl::init("funclist.txt"));
namespace {
struct  CollectCondBranchHints : public FunctionPass {
  static char ID;
  static FunctionMap *fMap;
  static int FID;
  static std::vector<std::string> *list;

  bool Modified = false;
//构造函数：初始化了一个 FunctionPass 类型的基类，并且传递了一个 ID 参数
  CollectCondBranchHints() : FunctionPass(ID) {
    list->clear();

    std::ifstream infile(InstrumentFunctionNameListFile);//通过命令行参数传递的 -funclist 选项指定的文件路径
    std::string word;
    //读取文件内容:函数名称
    while(infile>>word) {
        list->push_back(word);
    }
    if( list->empty()) {
        errs() << "Note: funclist is empty! use -funclist filename to designate funclist file" << "\n";
    }
  };
    //插桩核心代码：
  bool runOnFunction(Function &F) override {
    std::string name = F.getName().str();//获取函数名字

#ifdef INSTRUMENT_ALL // 如果宏 INSTRUMENT_ALL 被定义，则会对所有函数进行插桩
#else // 将根据一个外部提供的函数名列表（InstrumentFunctionNameListFile 参数指定的文件）决定是否对当前函数进行插桩。
    errs() << __func__ << " : "<< name << "\n";
    if (std::find(std::begin(*list), std::end(*list), name) == std::end(*list)) {
        //如果函数名称 name 不在 list 中（即函数不在需要插桩的函数列表里），则跳过该函数，输出信息并返回 false。
        errs() << __func__ << ": skip function "<< name << ", for it is not in list"<<"\n";
        return false;
    }
#endif
    // 检查我们是否之前访问过这个函数
    if (fMap->find(name) != fMap->end()) {
	(*fMap)[name] = ++FID; 
    }
    //遍历函数中的每个基本块
    for (BasicBlock &BB : F) {
        for  (Instruction &I : BB) {//每一个指令
            switch (I.getOpcode()) {//根据指令的操作码（opcode）来判断指令的类型
                case Instruction::Br: {//分支指令
                    BranchInst *bi = cast<BranchInst>(&I);
                    if (bi->isUnconditional()) {//无条件分支
                        //errs() << *bi << "\n";
                    } else {//有条件分支
                        //errs() <<"CondBranch: " << *bi << "\n";
                        Modified |= instrumentCondBranch(&I, bi->getCondition());//插桩
                        NumOfCondBranches++;
                    }
                    break;
                }

                default:
                   //errs() << I << "\n";
                   break;
            }
        }	
    }

    return Modified;
  }

  bool instrumentCondBranch(Instruction *I, Value *cond);//插桩代码

};
} // end of namespace

bool CollectCondBranchHints::instrumentCondBranch(Instruction *I, Value *cond) {
    IRBuilder<> B(I);//IRBuilder 会在指令 I 后面插入新的 IR 指令
    Module *M = B.GetInsertBlock()->getModule();
    Type *VoidTy = B.getVoidTy();
    Type *I1Ty = B.getInt1Ty();

    //errs() << __func__ << " : "<< *I<< "\n";

    Constant *ConstCollectCondBranchHints= M->getOrInsertFunction("__collect_cond_branch_hints", VoidTy,
                                                                     I1Ty, 
                                                                     nullptr);//插入函数声明

    Function *FuncCollectCondBranchHints= cast<Function>(ConstCollectCondBranchHints);

    B.CreateCall(FuncCollectCondBranchHints, {cond});//在条件分支处插入函数调用

    return true;

}

char CollectCondBranchHints::ID = 0;
int CollectCondBranchHints::FID = 0;
FunctionMap *CollectCondBranchHints::fMap= new FunctionMap();
std::vector<std::string> *CollectCondBranchHints::list = new std::vector<std::string>();
static RegisterPass<CollectCondBranchHints> X("collect-cond-branch-hints-pass", "Collect Conditional Branch Hints Info", false, false);
