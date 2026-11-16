add_llvm_component_library(LLVMZ80AsmParser
  Z80AsmParser.cpp

  LINK_COMPONENTS
  CodeGenTypes
  MC
  MCParser
  Support
  TargetParser
  Z80Desc
  Z80Info

  ADD_TO_COMPONENT
  Z80
  )
