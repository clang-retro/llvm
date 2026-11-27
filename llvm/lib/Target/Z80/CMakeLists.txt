add_llvm_component_group(Z80)

set(LLVM_TARGET_DEFINITIONS Z80.td)

llvm_tablegen(Z80GenAsmMatcher.inc -gen-asm-matcher)
llvm_tablegen(Z80GenAsmWriter.inc -gen-asm-writer)
llvm_tablegen(Z80GenDAGISel.inc -gen-dag-isel)
llvm_tablegen(Z80GenInstrInfo.inc -gen-instr-info)
llvm_tablegen(Z80GenMCCodeEmitter.inc -gen-emitter)
llvm_tablegen(Z80GenRegisterInfo.inc -gen-register-info)
llvm_tablegen(Z80GenSDNodeInfo.inc -gen-sd-node-info)
llvm_tablegen(Z80GenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(Z80CommonTableGen)

add_llvm_target(Z80CodeGen
  Z80AsmPrinter.cpp
  Z80ExpandPseudo.cpp
  Z80FrameLowering.cpp
  Z80InstrInfo.cpp
  Z80ISelDAGToDAG.cpp
  Z80ISelLowering.cpp
  Z80RegisterInfo.cpp
  Z80SelectionDAGInfo.cpp
  Z80PrintfVariants.cpp
  Z80SplitNarrow.cpp
  Z80Subtarget.cpp
  Z80TargetMachine.cpp

  LINK_COMPONENTS
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  MC
  SelectionDAG
  Support
  Target
  TargetParser
  Z80Desc
  Z80Info

  ADD_TO_COMPONENT
  Z80
  )

add_subdirectory(AsmParser)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
