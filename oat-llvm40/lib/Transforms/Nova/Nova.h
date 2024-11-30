#ifndef NOVA_H
#define NOVA_H
smndlasdlandsla shouldDiscardValueNamesas
ncalks cla static_cast
#include "llvm/ADT/SetVector.h"

#define MAX_RUNS    1

namespace llvm{
    class Module;

    struct AliasObject;
    struct AliasObjectTuple;
    struct GlobalState;
    struct CallInst;
    struct GEPOperator;
    struct GlobalValue;

    typedef SetVector<Value *> ValueSet;//Value集合
    typedef SetVector<Instruction *> InstSet;//指令集合向量

    /*AliasTuple
        AliasObjectTupleRef:别名元指针
        upleSet：别名元集合
        
        */
    typedef struct AliasObjectTuple *AliasObjectTupleRef;
    typedef SetVector<AliasObjectTupleRef> TupleSet;

    /*
        AliasObject成员
        1. AliasMapRef aliasMap
            AliasObjectRef:别名指针（存储别名的信息）；
            AliasObjectSet：存储唯一且有序、别名之间存在关系别名的集合
            AliasMap：
                int:不同的别名组；
                AliasObjectSet：具体的别名组信息
                        EG:键 1 可能表示指向同一内存位置的指针 a 和 b;键 2 可能表示独立的指针 c
            AliasMapRef:指向别名组的指针
        2.AliasMapRef aliasMap
            LocalTaintMap：
                int:不同的组号
                InstSet：指令集合

        */
    typedef struct AliasObject * AliasObjectRef;
    typedef SetVector<AliasObjectRef> AliasObjectSet;
    typedef std::map<int, AliasObjectSet *> AliasMap;
    typedef AliasMap *AliasMapRef;
    typedef std::map<int, InstSet *> LocalTaintMap;
    typedef LocalTaintMap *LocalTaintMapRef;

    /*
        GlobalState 成员:
            TaintMap:不同value对应的指令
            PointsToMap：不同value对应的别名元；
            VisitMap：当前value是否分析过
            */
    typedef std::map<Value*, InstSet *> TaintMap;
    typedef std::map<Value*, TupleSet *> PointsToMap;
    typedef std::map<Value*, bool> VisitMap;
    typedef TaintMap *TaintMapRef;
    typedef PointsToMap *PointsToMapRef;
    typedef VisitMap *VisitMapRef;

    typedef struct GlobalState *GlobalStateRef; 

    // 强联通分量：一个SCC的内容
    typedef std::vector<BasicBlock *> SCC;
    typedef std::vector<BasicBlock *> *SCCRef;

// 所有本地/全局变量和动态分配的对象应该有一个别名对象 
// 对于位置，.aliasMap = null, .taintMap = null, .val = Instruction 
// 对于动态分配的对象，size 用于记录分配的大小。

/*AliasObject：别名分析类结构体：
                别名分析的核心：判断不同的指针是否指向相同的内存区域。
                val：程序中的值（指令、常亮、变量）
                isStruct:是否是结构体;
                isLocation:若isLoacation为true，则val表示一个内存位置；
                type:val具体的类型；
                size:val在内存中的大小
                aliasMap:存储与该对象相关的别名关系（分组存储）
                tainMap:与该对象先关的污点指令（分组存储）
    AliasObjectTuple：
                offset:变量偏移量（如结构体中相对偏移量）
                AliasObject：不同变量的别名信息
    */
struct AliasObject {
    Value *val;
    bool isStruct;
    bool isLocation;
    Type *type;
    uint32_t size;
    AliasMapRef aliasMap;
    LocalTaintMapRef taintMap;
}; // struct AliasObject

struct AliasObjectTuple {
    int offset;
    struct AliasObject *ao;
}; // struct AliasObjectTuple
/*
    GlobalState：全局状态
        TaintMapRef：不同value对应的指令集合
        PointsToMapRef：不同value对应的别名元集合
        VisitMapRef：是否遍历过
        CallInst：当前的调用指令
        ValueSet：value集合
    */
struct GlobalState {
    TaintMapRef tMap;
    PointsToMapRef pMap;
    VisitMapRef vMap;   
    CallInst *ci;   
    ValueSet *senVarSet;
}; // GlobalState

struct Nova : public ModulePass {
    static char ID;
    Nova() : ModulePass(ID) {}
    //runOnModule(Module &M) 是 pass 的核心函数，它将在整个模块上运行
    bool runOnModule(Module &M);

    /*
        定义-使用检查:对变量的定义和使用情况的跟踪
            GetAnnotatedVariables：提取模块中已标注的变量。
            DefUseCheck：检查模块中定义和使用的关系，可能是为了分析某个变量的使用链。
            RecordDefineEvent：记录一个变量的定义事件。
            CheckUseEvent：检查一个变量的使用事件
        */
    void GetAnnotatedVariables(Module &M, GlobalStateRef gs);
    void DefUseCheck(Module &M, GlobalStateRef gs);
    void RecordDefineEvent(Module &M, Value *var);
    void CheckUseEvent(Module &M, Value *var);
    void InstrumentStoreInst(Instruction *inst, Value *addr, Value *val);
    void InstrumentLoadInst(Instruction *inst, Value *addr, Value *val);
    /*
        指针边界检查：指针相关的安全性检查
            ConstructCheckHandlers：构造指针边界检查的处理器，可能是设置或初始化与指针边界相关的检查逻辑。
            PointerBoundaryCheck：检查指针是否超出了预定的边界。
            PointerAccessCheck：检查指针访问是否合法，可能涉及验证指针是否指向合法的内存区域。
            ArrayAccessCheck：检查数组访问是否越界。
            CollectArrayBoundaryInfo：收集与数组边界相关的信息。
            GetInstIterator：获取指向指定变量的指令迭代器，可能用于定位或遍历指令。
        */
    void ConstructCheckHandlers(Module &M);
    void PointerBoundaryCheck(Module &M, ValueSet &vs);
    void PointerAccessCheck(Module &M, Value *v);
    void ArrayAccessCheck(Module &M, Value *v);
    void CollectArrayBoundaryInfo(Module &M, Value *v);
    BasicBlock::iterator GetInstIterator(Value *v);
    /*
        指针基准和边界处理：这些函数用于处理指针的基址和边界相关的信息。
            CastToVoidPtr：将指针转换为 void* 类型，通常用于处理通用指针。
            DissociateBaseBound：解除指针的基地址和边界关联。
            AssociateBaseBound：将指针与其基地址和边界关联起来。
            GetAssociatedBase 和 GetAssociatedBound：分别获取与指针关联的基地址和边界。
            IsStructOperand：判断指针操作数是否为结构体类型。
            GetSizeOfType：获取给定类型的大小。
        */
    Value* CastToVoidPtr(Value* operand, Instruction* insert_at);
    void DissociateBaseBound(Value* pointer_operand);
    void AssociateBaseBound(Value* pointer_operand, Value* pointer_base, Value* pointer_bound);
    Value* GetAssociatedBase(Value* pointer_operand);
    Value* GetAssociatedBound(Value* pointer_operand);
    bool IsStructOperand(Value* pointer_operand);
    Value* GetSizeOfType(Type* input_type);
    /*
        全局变量基址和边界管理：
            AddBaseBoundGlobalValue：将全局变量与基地址和边界关联。
            AddBaseBoundGlobals：将模块中的所有全局变量进行基地址和边界的关联。
            HandleGlobalSequentialTypeInitializer：处理全局变量初始化时涉及的顺序类型。
        */
    void AddBaseBoundGlobalValue(Module &M, Value *v);
    void AddBaseBoundGlobals(Module &M);
    void HandleGlobalSequentialTypeInitializer(Module& M, GlobalVariable* gv);
    /*
        存储、常量和结构体初始化相关处理：
            AddStoreBaseBoundFunc：将基地址和边界信息添加到存储操作中。
            GetConstantExprBaseBound：从常量表达式中获取基地址和边界。
            HandleGlobalStructTypeInitializer：处理全局结构体类型的初始化，涉及结构体类型、初始化常量等信息。
        */
    void AddStoreBaseBoundFunc(Value* pointer_dest, Value* pointer_base, Value* pointer_bound,
                                 Value* pointer, Value* size_of_type, Instruction* insert_at);
    void GetConstantExprBaseBound(Constant* given_constant, Value* & tmp_base, Value* & tmp_bound);
    void HandleGlobalStructTypeInitializer(Module& M, StructType* init_struct_type, Constant* initializer, 
                                  GlobalVariable* gv, std::vector<Constant*> indices_addr_ptr, int length);
    /*
        全局初始化和主函数处理：
            GetGlobalInitInstruction：获取全局变量的初始化指令。
            TransformMain：对主函数进行转换，可能是为了引入额外的检查或修改。
            GatherBaseBoundPass1 和 GatherBaseBoundPass2：两个阶段的处理，可能用于收集和处理函数中的基地址和边界信息。
        */
    Instruction* GetGlobalInitInstruction(Module& M);
    void TransformMain(Module& module);
    void GatherBaseBoundPass1(Function* func);
    void GatherBaseBoundPass2(Function* func);
    /*
        加载、存储和反向引用检查：
            AddLoadStoreChecks：为加载和存储指令添加检查，确保内存访问安全。
            AddDereferenceChecks：对函数中的反向引用进行检查。
            PrepareForBoundsCheck：为边界检查做准备，可能是初始化数据结构或变量。
            IdentifyFuncToTrans：识别需要转换的函数。
            CheckIfFunctionOfInterest：检查函数是否是我们关心的目标函数。
        */
    void AddLoadStoreChecks(Instruction* load_store, std::map<Value*, int>& FDCE_map);
    void AddDereferenceChecks(Function* func, ValueSet &vs);
    void PrepareForBoundsCheck(Module &M, ValueSet &vs);
    void IdentifyFuncToTrans(Module& module);
    bool CheckIfFunctionOfInterest(Function* func);
    bool CheckTypeHasPtrs(Argument* ptr_argument);
    bool CheckPtrsInST(StructType* struct_type);
    bool CheckBaseBoundMetadataPresent(Value* pointer_operand);
    bool HasPtrArgRetType(Function* func); 
    bool IsFuncDefSoftBound(const std::string &str);
    void IdentifyInitialGlobals(Module& module);
    void IdentifyOriginalInst(Function * func);
    void HandleAlloca (AllocaInst* alloca_inst, BasicBlock* bb, BasicBlock::iterator& i);
    void HandleLoad(LoadInst* load_inst);
    void HandleGEP(GetElementPtrInst* gep_inst);
    void HandleBitCast(BitCastInst* bitcast_inst);
    void HandleMemcpy(CallInst* call_inst);
    void HandleCallInst(CallInst* call_inst);
    void HandleSelect(SelectInst* select_ins, int pass);
    void HandlePHIPass1(PHINode* phi_node);
    void HandlePHIPass2(PHINode* phi_node);
    void HandleIntToPtr(IntToPtrInst* inttoptrinst);
    void HandleReturnInst(ReturnInst* ret);
    void HandleExtractElement(ExtractElementInst* EEI);
    void HandleExtractValue(ExtractValueInst* EVI);
    void HandleVectorStore(StoreInst* store_inst);
    void HandleStore(StoreInst* store_inst);
    void InsertMetadataLoad(LoadInst* load_inst);
    void PropagateMetadata(Value* pointer_operand, Instruction* inst, int instruction_type);
    void AddMemcopyMemsetCheck(CallInst* call_inst, Function* called_func);
    void IntroduceShadowStackAllocation(CallInst* call_inst);
    void IntroduceShadowStackDeallocation(CallInst* call_inst, Instruction* insert_at);
    void IntroduceShadowStackStores(Value* ptr_value, Instruction* insert_at, int arg_no);
    void IntroduceShadowStackLoads(Value* ptr_value, Instruction* insert_at, int arg_no);
    int GetNumPointerArgsAndReturn(CallInst* call_inst);
    void GetGlobalVariableBaseBound(Value* operand, Value* & operand_base, Value* & operand_bound);
    Instruction* GetNextInstruction(Instruction* I);
    void IterateCallSiteIntroduceShadowStackStores(CallInst* call_inst);

    // SCC traversal
    //遍历模块
    void Traversal(GlobalStateRef gs, Function *f);
    //给定一个函数，反转并返回该函数的强联通图
    void ReverseSCC(std::vector<SCCRef> &, Function *f);
    void VisitSCC(GlobalStateRef gs, SCC &scc); 
    //处理循环
    void HandleLoop(GlobalStateRef gs, SCC &scc);  
    //处理函数调用
    void HandleCall(GlobalStateRef gs, CallInst &I); 
    //返回使用-定义的最大长度 
    uint32_t LongestUseDefChain(SCC &scc);
    void DispatchClients(GlobalStateRef gs, Instruction &I);
    /*标记调用是否处理过*/
    Function *ResolveCall(GlobalStateRef gs, CallInst &I);

    // points to analysis & taint analysis framework
    void PointsToAnalysis(GlobalStateRef gs, Instruction &I);
    void TaintAnalysis(GlobalStateRef gs, Instruction &I);
    void InitializeGS(GlobalStateRef gs, Module &M);
    void InitializeFunction(GlobalStateRef gs, Function *f, CallInst &I);
    void PrintPointsToMap(GlobalStateRef gs);
    void PrintTaintMap(GlobalStateRef gs);

    // points to analysis rules
    void UpdatePtoAlloca(GlobalStateRef gs, Instruction &I);
    void UpdatePtoBinOp(GlobalStateRef gs, Instruction &I);
    void UpdatePtoLoad(GlobalStateRef gs, Instruction &I);
    void UpdatePtoStore(GlobalStateRef gs, Instruction &I);
    void UpdatePtoGEP(GlobalStateRef gs, Instruction &I);
    void UpdatePtoRet(GlobalStateRef gs, Instruction &I);
    void UpdatePtoBitCast(GlobalStateRef gs, Instruction &I);

    // points to analysis helper functions 
    void HandlePtoGEPOperator(GlobalStateRef gs, GEPOperator *op);
    AliasObject *CreateAliasObject(Type *type, Value *v);
    void PrintAliasObject(AliasObjectRef ao);
    bool SkipStructType(Type *type);

    // taint to analysis rules
    void UpdateTaintAlloca(GlobalStateRef gs, Instruction &I);
    void UpdateTaintBinOp(GlobalStateRef gs, Instruction &I);
    void UpdateTaintLoad(GlobalStateRef gs, Instruction &I);
    void UpdateTaintStore(GlobalStateRef gs, Instruction &I);
    void UpdateTaintGEP(GlobalStateRef gs, Instruction &I);
    void UpdateTaintRet(GlobalStateRef gs, Instruction &I);
    void UpdateTaintBitCast(GlobalStateRef gs, Instruction &I);
}; // struct Nova
} // namespace

#endif //NOVA_H
