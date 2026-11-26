add_llvm_component_group(MOS)

option(LLVM_MOS_EXPERIMENTAL_REGALLOC_DEFAULT
  "Enable experimental MOS register allocation by default" OFF)

set(LLVM_TARGET_DEFINITIONS MOS.td)

llvm_tablegen(MOSGenAsmMatcher.inc -gen-asm-matcher)
llvm_tablegen(MOSGenAsmWriter.inc -gen-asm-writer)
llvm_tablegen(MOSGenCallingConv.inc -gen-callingconv)
llvm_tablegen(MOSGenDAGISel.inc -gen-dag-isel)
llvm_tablegen(MOSGenDisassemblerTables.inc -gen-disassembler)
llvm_tablegen(MOSGenGICombiner.inc -gen-global-isel-combiner -combiners="MOSCombiner")
llvm_tablegen(MOSGenGlobalISel.inc -gen-global-isel)
llvm_tablegen(MOSGenInstrInfo.inc -gen-instr-info)
llvm_tablegen(MOSGenMCCodeEmitter.inc -gen-emitter)
llvm_tablegen(MOSGenMCPseudoLowering.inc -gen-pseudo-lowering)
llvm_tablegen(MOSGenRegisterBank.inc -gen-register-bank)
llvm_tablegen(MOSGenRegisterInfo.inc -gen-register-info)
llvm_tablegen(MOSGenSearchableTables.inc -gen-searchable-tables)
llvm_tablegen(MOSGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(MOSCommonTableGen)

add_llvm_target(MOSCodeGen
  MOSAsmPrinter.cpp
  MOSCallGraphUtils.cpp
  MOSCallLowering.cpp
  MOSCallingConv.cpp
  MOSCombiner.cpp
  MOSConventionalSSA.cpp
  MOSCopyOpt.cpp
  MOSFrameLowering.cpp
  MOSISelLowering.cpp
  MOSImagRegAssign.cpp
  MOSImagRegSpill.cpp
  MOSIndexIV.cpp
  MOSInlineAsmLowering.cpp
  MOSInsertCopies.cpp
  MOSInstrCost.cpp
  MOSInstrInfo.cpp
  MOSInstructionSelector.cpp
  MOSInternalize.cpp
  MOSLateOptimization.cpp
  MOSLegalizerInfo.cpp
  MOSInterferenceGraph.cpp
  MOSLowerSelect.cpp
  MOSMCInstLower.cpp
  MOSMachineScheduler.cpp
  MOSNonReentrant.cpp
  MOSPostRAScavenging.cpp
  MOSRegAlloc.cpp
  MOSRegisterBankInfo.cpp
  MOSRegisterInfo.cpp
  MOSShiftRotateChain.cpp
  MOSStaticStackAlloc.cpp
  MOSSubtarget.cpp
  MOSTargetMachine.cpp
  MOSTargetObjectFile.cpp
  MOSZeroPageAlloc.cpp

  DEPENDS
  intrinsics_gen

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  BinaryFormat
  CodeGen
  CodeGenTypes
  Core
  GlobalISel
  IPO
  InstCombine
  LTO
  MC
  MOSDesc
  MOSInfo
  Passes
  Scalar
  SelectionDAG
  Support
  Target
  TargetParser
  TransformUtils

  ADD_TO_COMPONENT
  MOS
)

target_compile_definitions(LLVMMOSCodeGen PRIVATE
  LLVM_MOS_EXPERIMENTAL_REGALLOC_DEFAULT=$<BOOL:${LLVM_MOS_EXPERIMENTAL_REGALLOC_DEFAULT}>)

add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
