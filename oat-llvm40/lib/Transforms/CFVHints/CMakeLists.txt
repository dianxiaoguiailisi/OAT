# If we don't need RTTI or EH, there's no reason to export anything
# from the hello plugin.
# 设置 LLVM 插件的符号导出文件，具体是在不需要运行时类型信息（RTTI，LLVM_REQUIRES_RTTI）和异常处理（EH，LLVM_REQUIRES_EH）的情况下，指定一个导出符号的文件 CollectLoopHints.exports
if( NOT LLVM_REQUIRES_RTTI )
  if( NOT LLVM_REQUIRES_EH )
    set(LLVM_EXPORTED_SYMBOL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/CollectLoopHints.exports)
  endif()
endif()

if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS Core Support)
endif()
# 创建加载模块
add_llvm_loadable_module( LLVMCollectCFVHints
  CollectLoopHints.cpp
  CollectCondBranchHints.cpp
  CollectICallHints.cpp
  CollectIBranchHints.cpp

  DEPENDS
  # 生成内建函数的工具
  intrinsics_gen
  # 插件构建工具
  PLUGIN_TOOL
  # LLVM 的优化工具
  opt
  )
