set(LLVM_LINK_COMPONENTS
  Support
  )

add_llvm_tool(llvm-exe2cmd
  llvm-exe2cmd.cpp
  )
