add_llvm_component_library(LLVMZ80Desc
  Z80AsmBackend.cpp
  Z80ELFObjectWriter.cpp
  Z80InstPrinter.cpp
  Z80MCAsmInfo.cpp
  Z80MCCodeEmitter.cpp
  Z80MCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  Support
  TargetParser
  Z80Info

  ADD_TO_COMPONENT
  Z80
  )
