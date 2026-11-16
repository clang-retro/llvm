//===-- Z80InstrInfo.td - Z80 and 8080 instructions --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The instructions use Zilog mnemonics for both processors. Those without a
// HasZ80 predicate are the 8080's.
//
//===----------------------------------------------------------------------===//

include "Z80InstrFormats.td"

//===----------------------------------------------------------------------===//
// Selection DAG nodes
//===----------------------------------------------------------------------===//

def SDT_Z80Call : SDTypeProfile<0, -1, [SDTCisVT<0, iPTR>]>;
def SDT_Z80CallSeqStart : SDCallSeqStart<[SDTCisVT<0, i16>,
                                          SDTCisVT<1, i16>]>;
def SDT_Z80CallSeqEnd : SDCallSeqEnd<[SDTCisVT<0, i16>, SDTCisVT<1, i16>]>;
def SDT_Z80Wrapper : SDTypeProfile<1, 1, [SDTCisSameAs<0, 1>,
                                          SDTCisPtrTy<0>]>;
def SDT_Z80Cmp : SDTypeProfile<0, 2, [SDTCisSameAs<0, 1>, SDTCisInt<0>]>;
def SDT_Z80Tst : SDTypeProfile<0, 1, [SDTCisInt<0>]>;
def SDT_Z80BrCond : SDTypeProfile<0, 2, [SDTCisVT<0, OtherVT>,
                                         SDTCisVT<1, i8>]>;
def SDT_Z80SelectCC : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                                           SDTCisSameAs<1, 2>,
                                           SDTCisVT<3, i8>]>;
def SDT_Z80Push : SDTypeProfile<0, 1, [SDTCisVT<0, i16>]>;
def SDT_Z80Shift : SDTypeProfile<1, 1, [SDTCisSameAs<0, 1>, SDTCisInt<0>]>;
def SDT_Z80Combine : SDTypeProfile<1, 2, [SDTCisVT<0, i16>, SDTCisVT<1, i8>,
                                          SDTCisVT<2, i8>]>;
def SDT_Z80Hi : SDTypeProfile<1, 1, [SDTCisVT<0, i8>, SDTCisVT<1, i16>]>;

// A call. Operand 1 is the callee, the rest the registers it takes.
def Z80call : SDNode<"Z80ISD::CALL", SDT_Z80Call,
                     [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                      SDNPVariadic]>;
// A return. The operands are the registers it returns.
def Z80ret : SDNode<"Z80ISD::RET", SDTNone,
                    [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def Z80callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_Z80CallSeqStart,
                              [SDNPHasChain, SDNPOutGlue]>;
def Z80callseq_end : SDNode<"ISD::CALLSEQ_END", SDT_Z80CallSeqEnd,
                            [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
// The address of a global, external symbol, jump table, block address or
// constant pool entry.
def Z80wrapper : SDNode<"Z80ISD::WRAPPER", SDT_Z80Wrapper>;
// Compare two values, setting the flags: carry is unsigned less than, and
// for 8-bit values zero is equality.
def Z80cmp : SDNode<"Z80ISD::CMP", SDT_Z80Cmp, [SDNPOutGlue]>;
// Set the zero flag if the value is zero.
def Z80tst : SDNode<"Z80ISD::TST", SDT_Z80Tst, [SDNPOutGlue]>;
// Branch on condition code operand 2 of the flags.
def Z80brcond : SDNode<"Z80ISD::BRCOND", SDT_Z80BrCond,
                       [SDNPHasChain, SDNPInGlue]>;
// Operand 0 if condition code operand 2 holds, else operand 1.
def Z80selectcc : SDNode<"Z80ISD::SELECT_CC", SDT_Z80SelectCC, [SDNPInGlue]>;
// Push an outgoing argument.
def Z80push : SDNode<"Z80ISD::PUSH", SDT_Z80Push, [SDNPHasChain, SDNPMayStore]>;
// Shifts by one bit.
def Z80shl1 : SDNode<"Z80ISD::SHL1", SDT_Z80Shift>;
def Z80srl1 : SDNode<"Z80ISD::SRL1", SDT_Z80Shift>;
def Z80sra1 : SDNode<"Z80ISD::SRA1", SDT_Z80Shift>;
// An i16 from its low and high bytes.
def Z80combine : SDNode<"Z80ISD::COMBINE", SDT_Z80Combine>;
// The high byte of an i16.
def Z80hi : SDNode<"Z80ISD::HI", SDT_Z80Hi>;

//===----------------------------------------------------------------------===//
// Predicates and operands
//===----------------------------------------------------------------------===//

def HasZ80 : Predicate<"Subtarget->hasZ80()">,
             AssemblerPredicate<(all_of FeatureZ80), "Z80">;

def imm8 : Operand<i8> {
  let EncoderMethod = "getImm8OpValue";
  let OperandType = "OPERAND_IMMEDIATE";
}
def imm16 : Operand<i16> {
  let EncoderMethod = "getImm16OpValue";
  let OperandType = "OPERAND_IMMEDIATE";
}
// An absolute address, of data or code.
def addr16 : Operand<i16> {
  let EncoderMethod = "getImm16OpValue";
}
def brtarget : Operand<OtherVT> {
  let EncoderMethod = "getImm16OpValue";
  let OperandType = "OPERAND_PCREL";
}
def calltarget : Operand<i16> {
  let EncoderMethod = "getImm16OpValue";
}
// The target of a relative jump.
def pcrel8 : Operand<OtherVT> {
  let EncoderMethod = "getPCRel8OpValue";
  let OperandType = "OPERAND_PCREL";
}
// The signed displacement of (IX+d) and (IY+d).
def disp8 : Operand<i8> {
  let PrintMethod = "printDisp";
  let EncoderMethod = "getDisp8OpValue";
}
def bitnum : Operand<i8>;
def rstvec : Operand<i8> {
  let EncoderMethod = "getRstOpValue";
}
def immode : Operand<i8> {
  let EncoderMethod = "getIMOpValue";
}

def CondCodeAsmOperand : AsmOperandClass { let Name = "CondCode"; }
def JRCondCodeAsmOperand : AsmOperandClass {
  let Name = "JRCondCode";
  let SuperClasses = [CondCodeAsmOperand];
}
// A condition: nz, z, nc, c, po, pe, p, m.
def ccode : Operand<i8> {
  let PrintMethod = "printCondCode";
  let ParserMatchClass = CondCodeAsmOperand;
}
// A condition of JR: nz, z, nc, c.
def jrccode : Operand<i8> {
  let PrintMethod = "printCondCode";
  let ParserMatchClass = JRCondCodeAsmOperand;
}

// A stack object: a frame index, or SP after frame index elimination, and an
// offset.
def memfi : Operand<iPTR> {
  let MIOperandInfo = (ops i16imm, i16imm);
}
def addrFI : ComplexPattern<iPTR, 2, "SelectFrameAddr", [frameindex], []>;
// An absolute address: a symbol or a constant.
def absaddr : ComplexPattern<iPTR, 1, "SelectAbsAddr", [], []>;

//===----------------------------------------------------------------------===//
// 8-bit loads
//===----------------------------------------------------------------------===//

let hasSideEffects = 0 in {
let isMoveReg = 1 in
def LDrr : Z80Inst<1, (outs GR8:$dst), (ins GR8:$src), "ld\t$dst, $src", []> {
  bits<3> dst;
  bits<3> src;
  let Inst{7-6} = 0b01;
  let Inst{5-3} = dst;
  let Inst{2-0} = src;
}

let isReMaterializable = 1, isAsCheapAsAMove = 1, isMoveImm = 1 in
def LDri : Z80Inst<2, (outs GR8:$dst), (ins imm8:$n), "ld\t$dst, $n",
                   [(set i8:$dst, imm:$n)]> {
  bits<3> dst;
  bits<8> n;
  let Inst{15-14} = 0b00;
  let Inst{13-11} = dst;
  let Inst{10-8} = 0b110;
  let Inst{7-0} = n;
}

let mayLoad = 1 in {
def LDrHL : Z80Inst<1, (outs GR8:$dst), (ins HL16:$ptr), "ld\t$dst, ($ptr)",
                    [(set i8:$dst, (load HL16:$ptr))]> {
  bits<3> dst;
  let Inst{7-6} = 0b01;
  let Inst{5-3} = dst;
  let Inst{2-0} = 0b110;
}
def LDArp : Z80Inst<1, (outs ACC:$dst), (ins BCDE:$ptr), "ld\t$dst, ($ptr)",
                    []> {
  bits<2> ptr;
  let Inst{7-6} = 0b00;
  let Inst{5-4} = ptr;
  let Inst{3-0} = 0b1010;
}
def LDAnn : Inn<3, 0x3A, (outs ACC:$dst), (ins addr16:$nn), "ld\t$dst, ($nn)",
                [(set i8:$dst, (load absaddr:$nn))]>;
}

let mayStore = 1 in {
def LDHLr : Z80Inst<1, (outs), (ins HL16:$ptr, GR8:$src), "ld\t($ptr), $src",
                    [(store i8:$src, HL16:$ptr)]> {
  bits<3> src;
  let Inst{7-3} = 0b01110;
  let Inst{2-0} = src;
}
def LDHLn : In<2, 0x36, (outs), (ins HL16:$ptr, imm8:$n), "ld\t($ptr), $n",
               [(store (i8 imm:$n), HL16:$ptr)]>;
def LDrpA : Z80Inst<1, (outs), (ins BCDE:$ptr, ACC:$src), "ld\t($ptr), $src",
                    []> {
  bits<2> ptr;
  let Inst{7-6} = 0b00;
  let Inst{5-4} = ptr;
  let Inst{3-0} = 0b0010;
}
def LDnnA : Inn<3, 0x32, (outs), (ins addr16:$nn, ACC:$src), "ld\t($nn), $src",
                [(store i8:$src, absaddr:$nn)]>;
}
} // hasSideEffects = 0

//===----------------------------------------------------------------------===//
// 16-bit loads
//===----------------------------------------------------------------------===//

let hasSideEffects = 0 in {
let isReMaterializable = 1, isAsCheapAsAMove = 1, isMoveImm = 1 in
def LD16ri : Z80Inst<3, (outs GR16:$dst), (ins imm16:$nn), "ld\t$dst, $nn",
                     [(set i16:$dst, imm:$nn)]> {
  bits<2> dst;
  bits<16> nn;
  let Inst{23-22} = 0b00;
  let Inst{21-20} = dst;
  let Inst{19-16} = 0b0001;
  let Inst{15-8} = nn{7-0};
  let Inst{7-0} = nn{15-8};
}
def LDSPnn : Inn<3, 0x31, (outs SPR:$dst), (ins imm16:$nn), "ld\t$dst, $nn">;

let mayLoad = 1 in
def LDHLnn : Inn<3, 0x2A, (outs HL16:$dst), (ins addr16:$nn),
                 "ld\t$dst, ($nn)", [(set i16:$dst, (load absaddr:$nn))]>;
let mayStore = 1 in
def LDnnHL : Inn<3, 0x22, (outs), (ins addr16:$nn, HL16:$src),
                 "ld\t($nn), $src", [(store i16:$src, absaddr:$nn)]>;

let Defs = [SP] in
def LDSPHL : I<1, 0xF9, (outs SPR:$dst), (ins HL16:$src), "ld\t$dst, $src">;

let Defs = [DE, HL], Uses = [DE, HL] in
def EXDEHL : I<1, 0xEB, (outs), (ins), "ex\tde, hl">;

let Defs = [HL], Uses = [HL, SP], mayLoad = 1, mayStore = 1 in
def EXSPHL : I<1, 0xE3, (outs), (ins), "ex\t(sp), hl">;

let Defs = [SP], Uses = [SP] in {
let mayStore = 1 in {
def PUSHrp : Z80Inst<1, (outs), (ins GR16:$rp), "push\t$rp",
                     [(Z80push i16:$rp)]> {
  bits<2> rp;
  let Inst{7-6} = 0b11;
  let Inst{5-4} = rp;
  let Inst{3-0} = 0b0101;
}
def PUSHAF : I<1, 0xF5, (outs), (ins AFR:$rp), "push\t$rp">;
}
let mayLoad = 1 in {
def POPrp : Z80Inst<1, (outs GR16:$rp), (ins), "pop\t$rp", []> {
  bits<2> rp;
  let Inst{7-6} = 0b11;
  let Inst{5-4} = rp;
  let Inst{3-0} = 0b0001;
}
def POPAF : I<1, 0xF1, (outs AFR:$rp), (ins), "pop\t$rp">;
}
}
} // hasSideEffects = 0

//===----------------------------------------------------------------------===//
// 8-bit arithmetic and logic
//===----------------------------------------------------------------------===//

// The operations of A with a register, an immediate or (HL). ADD, ADC and
// SBC are written with "a,", the others without.
multiclass ALU8<bits<3> Op, string Mn, string Dst, SDPatternOperator Node,
                bit Comm> {
  let Constraints = "$a = $dst" in {
    // Not marked commutable: commuting would put the value in A into the
    // other operand, where it gets in the way of the one tied to A.
    def r : Z80Inst<1, (outs ACC:$dst), (ins ACC:$a, GR8:$b),
                    Mn # "\t" # Dst # "$b",
                    [(set i8:$dst, (Node i8:$a, i8:$b))]> {
      bits<3> b;
      let Inst{7-6} = 0b10;
      let Inst{5-3} = Op;
      let Inst{2-0} = b;
    }
    def n : Z80Inst<2, (outs ACC:$dst), (ins ACC:$a, imm8:$n),
                    Mn # "\t" # Dst # "$n",
                    [(set i8:$dst, (Node i8:$a, imm:$n))]> {
      bits<8> n;
      let Inst{15-14} = 0b11;
      let Inst{13-11} = Op;
      let Inst{10-8} = 0b110;
      let Inst{7-0} = n;
    }
    let mayLoad = 1 in
    def HL : Z80Inst<1, (outs ACC:$dst), (ins ACC:$a, HL16:$ptr),
                     Mn # "\t" # Dst # "($ptr)", []> {
      let Inst{7-6} = 0b10;
      let Inst{5-3} = Op;
      let Inst{2-0} = 0b110;
    }
  }
}

let hasSideEffects = 0, Defs = [F] in {
defm ADD : ALU8<0b000, "add", "a, ", add, 1>;
defm SUB : ALU8<0b010, "sub", "", sub, 0>;
defm AND : ALU8<0b100, "and", "", and, 1>;
defm XOR : ALU8<0b101, "xor", "", xor, 1>;
defm OR  : ALU8<0b110, "or", "", or, 1>;
let Uses = [F] in {
defm ADC : ALU8<0b001, "adc", "a, ", adde, 0>;
defm SBC : ALU8<0b011, "sbc", "a, ", sube, 0>;
}

// CP compares A with its operand.
def CPr : Z80Inst<1, (outs), (ins ACC:$a, GR8:$b), "cp\t$b",
                  [(Z80cmp i8:$a, i8:$b)]> {
  bits<3> b;
  let Inst{7-3} = 0b10111;
  let Inst{2-0} = b;
}
def CPn : In<2, 0xFE, (outs), (ins ACC:$a, imm8:$n), "cp\t$n",
             [(Z80cmp i8:$a, imm:$n)]>;
let mayLoad = 1 in
def CPHL : I<1, 0xBE, (outs), (ins ACC:$a, HL16:$ptr), "cp\t($ptr)", []>;
// OR A: test A for zero.
let isCodeGenOnly = 1 in
def TSTA : I<1, 0xB7, (outs), (ins ACC:$a), "or\ta", [(Z80tst i8:$a)]>;

let Constraints = "$src = $dst" in {
def INCr : Z80Inst<1, (outs GR8:$dst), (ins GR8:$src), "inc\t$dst",
                   [(set i8:$dst, (add i8:$src, 1))]> {
  bits<3> dst;
  let Inst{7-6} = 0b00;
  let Inst{5-3} = dst;
  let Inst{2-0} = 0b100;
}
def DECr : Z80Inst<1, (outs GR8:$dst), (ins GR8:$src), "dec\t$dst",
                   [(set i8:$dst, (add i8:$src, -1))]> {
  bits<3> dst;
  let Inst{7-6} = 0b00;
  let Inst{5-3} = dst;
  let Inst{2-0} = 0b101;
}
}
let mayLoad = 1, mayStore = 1 in {
def INCHL : I<1, 0x34, (outs), (ins HL16:$ptr), "inc\t($ptr)">;
def DECHL : I<1, 0x35, (outs), (ins HL16:$ptr), "dec\t($ptr)">;
}

let Constraints = "$src = $dst" in {
def RLCA : I<1, 0x07, (outs ACC:$dst), (ins ACC:$src), "rlca">;
def RRCA : I<1, 0x0F, (outs ACC:$dst), (ins ACC:$src), "rrca">;
let Uses = [F] in {
def RLA : I<1, 0x17, (outs ACC:$dst), (ins ACC:$src), "rla">;
def RRA : I<1, 0x1F, (outs ACC:$dst), (ins ACC:$src), "rra">;
}
def DAA : I<1, 0x27, (outs ACC:$dst), (ins ACC:$src), "daa">;
def CPL : I<1, 0x2F, (outs ACC:$dst), (ins ACC:$src), "cpl",
            [(set i8:$dst, (not i8:$src))]>;
}
def SCF : I<1, 0x37, (outs), (ins), "scf">;
let Uses = [F] in
def CCF : I<1, 0x3F, (outs), (ins), "ccf">;
} // hasSideEffects = 0, Defs = [F]

//===----------------------------------------------------------------------===//
// 16-bit arithmetic
//===----------------------------------------------------------------------===//

let hasSideEffects = 0 in {
let Defs = [F], Constraints = "$a = $dst" in {
def ADDHLrp : Z80Inst<1, (outs HL16:$dst), (ins HL16:$a, GR16:$b),
                      "add\t$dst, $b",
                      [(set i16:$dst, (add i16:$a, i16:$b))]> {
  bits<2> b;
  let Inst{7-6} = 0b00;
  let Inst{5-4} = b;
  let Inst{3-0} = 0b1001;
}
def ADDHLSP : I<1, 0x39, (outs HL16:$dst), (ins HL16:$a, SPR:$b),
                "add\t$dst, $b">;
}

let Constraints = "$src = $dst" in {
def INC16 : Z80Inst<1, (outs GR16:$dst), (ins GR16:$src), "inc\t$dst",
                    [(set i16:$dst, (add i16:$src, 1))]> {
  bits<2> dst;
  let Inst{7-6} = 0b00;
  let Inst{5-4} = dst;
  let Inst{3-0} = 0b0011;
}
def DEC16 : Z80Inst<1, (outs GR16:$dst), (ins GR16:$src), "dec\t$dst",
                    [(set i16:$dst, (add i16:$src, -1))]> {
  bits<2> dst;
  let Inst{7-6} = 0b00;
  let Inst{5-4} = dst;
  let Inst{3-0} = 0b1011;
}
}
let Defs = [SP], Uses = [SP] in {
def INCSP : I<1, 0x33, (outs), (ins), "inc\tsp">;
def DECSP : I<1, 0x3B, (outs), (ins), "dec\tsp">;
}
} // hasSideEffects = 0

//===----------------------------------------------------------------------===//
// Control flow
//===----------------------------------------------------------------------===//

let hasSideEffects = 0 in
def NOP : I<1, 0x00, (outs), (ins), "nop">;
let hasSideEffects = 1 in {
def HALT : I<1, 0x76, (outs), (ins), "halt">;
def DI : I<1, 0xF3, (outs), (ins), "di">;
def EI : I<1, 0xFB, (outs), (ins), "ei">;
def INAn : In<2, 0xDB, (outs ACC:$dst), (ins imm8:$n), "in\t$dst, ($n)">;
def OUTnA : In<2, 0xD3, (outs), (ins imm8:$n, ACC:$src), "out\t($n), $src">;
}

let isBranch = 1, isTerminator = 1 in {
let isBarrier = 1 in {
def JP : Inn<3, 0xC3, (outs), (ins brtarget:$nn), "jp\t$nn", [(br bb:$nn)]>;
let isIndirectBranch = 1 in
def JPHL : I<1, 0xE9, (outs), (ins HL16:$ptr), "jp\t($ptr)",
             [(brind HL16:$ptr)]>;
}
let Uses = [F] in
def JPcc : Z80Inst<3, (outs), (ins ccode:$cc, brtarget:$nn), "jp\t$cc, $nn",
                   [(Z80brcond bb:$nn, (i8 timm:$cc))]> {
  bits<3> cc;
  bits<16> nn;
  let Inst{23-22} = 0b11;
  let Inst{21-19} = cc;
  let Inst{18-16} = 0b010;
  let Inst{15-8} = nn{7-0};
  let Inst{7-0} = nn{15-8};
}
}

let isCall = 1, Uses = [SP], Defs = [SP] in {
def CALL : Inn<3, 0xCD, (outs), (ins calltarget:$nn, variable_ops),
               "call\t$nn">;
let Uses = [SP, F] in
def CALLcc : Z80Inst<3, (outs), (ins ccode:$cc, calltarget:$nn),
                     "call\t$cc, $nn"> {
  bits<3> cc;
  bits<16> nn;
  let Inst{23-22} = 0b11;
  let Inst{21-19} = cc;
  let Inst{18-16} = 0b100;
  let Inst{15-8} = nn{7-0};
  let Inst{7-0} = nn{15-8};
}
def RST : Z80Inst<1, (outs), (ins rstvec:$p), "rst\t$p"> {
  bits<3> p;
  let Inst{7-6} = 0b11;
  let Inst{5-3} = p;
  let Inst{2-0} = 0b111;
}
}

let isReturn = 1, isTerminator = 1, Uses = [SP], Defs = [SP] in {
let isBarrier = 1 in
def RET : I<1, 0xC9, (outs), (ins), "ret", [(Z80ret)]>;
let Uses = [SP, F] in
def RETcc : Z80Inst<1, (outs), (ins ccode:$cc), "ret\t$cc"> {
  bits<3> cc;
  let Inst{7-6} = 0b11;
  let Inst{5-3} = cc;
  let Inst{2-0} = 0b000;
}
}

//===----------------------------------------------------------------------===//
// Z80 instructions
//===----------------------------------------------------------------------===//

let Predicates = [HasZ80] in {

let isBranch = 1, isTerminator = 1, hasSideEffects = 0 in {
let isBarrier = 1 in
def JR : In<2, 0x18, (outs), (ins pcrel8:$n), "jr\t$n">;
let Uses = [F] in
def JRcc : Z80Inst<2, (outs), (ins jrccode:$cc, pcrel8:$n), "jr\t$cc, $n"> {
  bits<2> cc;
  bits<8> n;
  let Inst{15-13} = 0b001;
  let Inst{12-11} = cc;
  let Inst{10-8} = 0b000;
  let Inst{7-0} = n;
}
let Defs = [B], Uses = [B] in
def DJNZ : In<2, 0x10, (outs), (ins pcrel8:$n), "djnz\t$n">;
}

let hasSideEffects = 0 in {
let Defs = [BC, DE, HL], Uses = [BC, DE, HL] in
def EXX : I<1, 0xD9, (outs), (ins), "exx">;
}

// The CB group: rotates, shifts and bit operations on a register or (HL).
multiclass CBShift<bits<3> Op, string Mn> {
  let Defs = [F], Constraints = "$src = $dst" in
  def r : Z80Inst<2, (outs GR8:$dst), (ins GR8:$src), Mn # "\t$dst"> {
    bits<3> dst;
    let Inst{15-8} = 0xCB;
    let Inst{7-6} = 0b00;
    let Inst{5-3} = Op;
    let Inst{2-0} = dst;
  }
  let Defs = [F], mayLoad = 1, mayStore = 1 in
  def HL : I<2, !or(0xCB00, !shl(Op, 3), 0b110), (outs), (ins HL16:$ptr),
             Mn # "\t($ptr)">;
}
let hasSideEffects = 0 in {
defm RLC : CBShift<0b000, "rlc">;
defm RRC : CBShift<0b001, "rrc">;
let Uses = [F] in {
defm RL : CBShift<0b010, "rl">;
defm RR : CBShift<0b011, "rr">;
}
defm SLA : CBShift<0b100, "sla">;
defm SRA : CBShift<0b101, "sra">;
defm SRL : CBShift<0b111, "srl">;

let Defs = [F] in {
def BITr : Z80Inst<2, (outs), (ins bitnum:$b, GR8:$r), "bit\t$b, $r"> {
  bits<3> b;
  bits<3> r;
  let Inst{15-8} = 0xCB;
  let Inst{7-6} = 0b01;
  let Inst{5-3} = b;
  let Inst{2-0} = r;
}
let mayLoad = 1 in
def BITHL : Z80Inst<2, (outs), (ins bitnum:$b, HL16:$ptr), "bit\t$b, ($ptr)"> {
  bits<3> b;
  let Inst{15-8} = 0xCB;
  let Inst{7-6} = 0b01;
  let Inst{5-3} = b;
  let Inst{2-0} = 0b110;
}
}
multiclass CBBit<bits<2> Op, string Mn> {
  let Constraints = "$src = $dst" in
  def r : Z80Inst<2, (outs GR8:$dst), (ins bitnum:$b, GR8:$src),
                  Mn # "\t$b, $dst"> {
    bits<3> b;
    bits<3> dst;
    let Inst{15-8} = 0xCB;
    let Inst{7-6} = Op;
    let Inst{5-3} = b;
    let Inst{2-0} = dst;
  }
  let mayLoad = 1, mayStore = 1 in
  def HL : Z80Inst<2, (outs), (ins bitnum:$b, HL16:$ptr),
                   Mn # "\t$b, ($ptr)"> {
    bits<3> b;
    let Inst{15-8} = 0xCB;
    let Inst{7-6} = Op;
    let Inst{5-3} = b;
    let Inst{2-0} = 0b110;
  }
}
defm RES : CBBit<0b10, "res">;
defm SET : CBBit<0b11, "set">;
} // hasSideEffects = 0

// The ED group.
let hasSideEffects = 0 in {
let Defs = [F], Uses = [F], Constraints = "$a = $dst" in {
def SBCHLrp : Z80Inst<2, (outs HL16:$dst), (ins HL16:$a, GR16:$b),
                      "sbc\t$dst, $b"> {
  bits<2> b;
  let Inst{15-8} = 0xED;
  let Inst{7-6} = 0b01;
  let Inst{5-4} = b;
  let Inst{3-0} = 0b0010;
}
def ADCHLrp : Z80Inst<2, (outs HL16:$dst), (ins HL16:$a, GR16:$b),
                      "adc\t$dst, $b"> {
  bits<2> b;
  let Inst{15-8} = 0xED;
  let Inst{7-6} = 0b01;
  let Inst{5-4} = b;
  let Inst{3-0} = 0b1010;
}
def SBCHLSP : I<2, 0xED72, (outs HL16:$dst), (ins HL16:$a, SPR:$b),
                "sbc\t$dst, $b">;
def ADCHLSP : I<2, 0xED7A, (outs HL16:$dst), (ins HL16:$a, SPR:$b),
                "adc\t$dst, $b">;
}
let Defs = [F], Constraints = "$src = $dst" in
def NEG : I<2, 0xED44, (outs ACC:$dst), (ins ACC:$src), "neg",
            [(set i8:$dst, (ineg i8:$src))]>;

let mayLoad = 1 in
def LDrpnn : Z80Inst<4, (outs BCDE:$dst), (ins addr16:$nn),
                     "ld\t$dst, ($nn)"> {
  bits<2> dst;
  bits<16> nn;
  let Inst{31-24} = 0xED;
  let Inst{23-22} = 0b01;
  let Inst{21-20} = dst;
  let Inst{19-16} = 0b1011;
  let Inst{15-8} = nn{7-0};
  let Inst{7-0} = nn{15-8};
}
let mayStore = 1 in
def LDnnrp : Z80Inst<4, (outs), (ins addr16:$nn, BCDE:$src),
                     "ld\t($nn), $src"> {
  bits<2> src;
  bits<16> nn;
  let Inst{31-24} = 0xED;
  let Inst{23-22} = 0b01;
  let Inst{21-20} = src;
  let Inst{19-16} = 0b0011;
  let Inst{15-8} = nn{7-0};
  let Inst{7-0} = nn{15-8};
}
let mayLoad = 1 in
def LDSPnnind : Inn<4, 0xED7B, (outs SPR:$dst), (ins addr16:$nn),
                    "ld\t$dst, ($nn)">;
let mayStore = 1 in
def LDnnSP : Inn<4, 0xED73, (outs), (ins addr16:$nn, SPR:$src),
                 "ld\t($nn), $src">;
}

// Block moves and searches.
let mayLoad = 1, mayStore = 1, Defs = [BC, DE, HL, F],
    Uses = [BC, DE, HL] in {
def LDI : I<2, 0xEDA0, (outs), (ins), "ldi">;
def LDD : I<2, 0xEDA8, (outs), (ins), "ldd">;
def LDIR : I<2, 0xEDB0, (outs), (ins), "ldir">;
def LDDR : I<2, 0xEDB8, (outs), (ins), "lddr">;
}
let mayLoad = 1, Defs = [BC, HL, F], Uses = [A, BC, HL] in {
def CPI : I<2, 0xEDA1, (outs), (ins), "cpi">;
def CPD : I<2, 0xEDA9, (outs), (ins), "cpd">;
def CPIR : I<2, 0xEDB1, (outs), (ins), "cpir">;
def CPDR : I<2, 0xEDB9, (outs), (ins), "cpdr">;
}

let hasSideEffects = 1 in {
let isReturn = 1, isTerminator = 1, isBarrier = 1, Uses = [SP],
    Defs = [SP] in {
def RETI : I<2, 0xED4D, (outs), (ins), "reti">;
def RETN : I<2, 0xED45, (outs), (ins), "retn">;
}
def IM : Z80Inst<2, (outs), (ins immode:$m), "im\t$m"> {
  bits<2> m;
  let Inst{15-8} = 0xED;
  let Inst{7-5} = 0b010;
  let Inst{4-3} = m;
  let Inst{2-0} = 0b110;
}
}

// IX and IY instructions, behind a DD or FD prefix.
multiclass IndexInsts<int Pfx, RegisterClass XYC> {
  defvar P = !shl(Pfx, 8);
  let hasSideEffects = 0 in {
  def LDri : Inn<4, !or(P, 0x21), (outs XYC:$dst), (ins imm16:$nn),
                 "ld\t$dst, $nn">;
  let mayLoad = 1 in
  def LDnn : Inn<4, !or(P, 0x2A), (outs XYC:$dst), (ins addr16:$nn),
                 "ld\t$dst, ($nn)">;
  let mayStore = 1 in
  def STnn : Inn<4, !or(P, 0x22), (outs), (ins addr16:$nn, XYC:$src),
                 "ld\t($nn), $src">;
  let Defs = [F], Constraints = "$a = $dst" in {
  def ADDrp : Z80Inst<2, (outs XYC:$dst), (ins XYC:$a, BCDE:$b),
                      "add\t$dst, $b"> {
    bits<2> b;
    let Inst{15-8} = Pfx;
    let Inst{7-6} = 0b00;
    let Inst{5-4} = b;
    let Inst{3-0} = 0b1001;
  }
  def ADDself : I<2, !or(P, 0x29), (outs XYC:$dst), (ins XYC:$a, XYC:$b),
                  "add\t$dst, $b">;
  def ADDSP : I<2, !or(P, 0x39), (outs XYC:$dst), (ins XYC:$a, SPR:$b),
                "add\t$dst, $b">;
  }
  let Constraints = "$src = $dst" in {
  def INC : I<2, !or(P, 0x23), (outs XYC:$dst), (ins XYC:$src), "inc\t$dst">;
  def DEC : I<2, !or(P, 0x2B), (outs XYC:$dst), (ins XYC:$src), "dec\t$dst">;
  }
  let Defs = [SP], Uses = [SP] in {
  let mayStore = 1 in
  def PUSH : I<2, !or(P, 0xE5), (outs), (ins XYC:$src), "push\t$src">;
  let mayLoad = 1 in
  def POP : I<2, !or(P, 0xE1), (outs XYC:$dst), (ins), "pop\t$dst">;
  def LDSP : I<2, !or(P, 0xF9), (outs SPR:$dst), (ins XYC:$src),
               "ld\t$dst, $src">;
  }
  let Uses = [SP], mayLoad = 1, mayStore = 1, Constraints = "$src = $dst" in
  def EXSP : I<2, !or(P, 0xE3), (outs XYC:$dst), (ins XYC:$src),
               "ex\t(sp), $dst">;
  let mayLoad = 1 in
  def LDrd : Z80Inst<3, (outs GR8:$dst), (ins XYC:$xy, disp8:$d),
                     "ld\t$dst, ($xy$d)"> {
    bits<3> dst;
    bits<8> d;
    let Inst{23-16} = Pfx;
    let Inst{15-14} = 0b01;
    let Inst{13-11} = dst;
    let Inst{10-8} = 0b110;
    let Inst{7-0} = d;
  }
  let mayStore = 1 in {
  def STdr : Z80Inst<3, (outs), (ins XYC:$xy, disp8:$d, GR8:$src),
                     "ld\t($xy$d), $src"> {
    bits<3> src;
    bits<8> d;
    let Inst{23-16} = Pfx;
    let Inst{15-11} = 0b01110;
    let Inst{10-8} = src;
    let Inst{7-0} = d;
  }
  def STdn : Z80Inst<4, (outs), (ins XYC:$xy, disp8:$d, imm8:$n),
                     "ld\t($xy$d), $n"> {
    bits<8> d;
    bits<8> n;
    let Inst{31-24} = Pfx;
    let Inst{23-16} = 0x36;
    let Inst{15-8} = d;
    let Inst{7-0} = n;
  }
  }
  let Defs = [F], mayLoad = 1, mayStore = 1 in {
  def INCd : Z80Inst<3, (outs), (ins XYC:$xy, disp8:$d), "inc\t($xy$d)"> {
    bits<8> d;
    let Inst{23-16} = Pfx;
    let Inst{15-8} = 0x34;
    let Inst{7-0} = d;
  }
  def DECd : Z80Inst<3, (outs), (ins XYC:$xy, disp8:$d), "dec\t($xy$d)"> {
    bits<8> d;
    let Inst{23-16} = Pfx;
    let Inst{15-8} = 0x35;
    let Inst{7-0} = d;
  }
  }
  } // hasSideEffects = 0
  let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1 in
  def JP : I<2, !or(P, 0xE9), (outs), (ins XYC:$ptr), "jp\t($ptr)">;
}
defm IX : IndexInsts<0xDD, IXR>;
defm IY : IndexInsts<0xFD, IYR>;

// The operations of A with (IX+d) and (IY+d).
multiclass IndexALU<bits<3> Op, string Mn, string Dst> {
  let Defs = [F], mayLoad = 1, hasSideEffects = 0 in {
  let Constraints = "$a = $dst" in {
  def IXd : Z80Inst<3, (outs ACC:$dst), (ins ACC:$a, IXR:$xy, disp8:$d),
                    Mn # "\t" # Dst # "($xy$d)"> {
    bits<8> d;
    let Inst{23-16} = 0xDD;
    let Inst{15-14} = 0b10;
    let Inst{13-11} = Op;
    let Inst{10-8} = 0b110;
    let Inst{7-0} = d;
  }
  def IYd : Z80Inst<3, (outs ACC:$dst), (ins ACC:$a, IYR:$xy, disp8:$d),
                    Mn # "\t" # Dst # "($xy$d)"> {
    bits<8> d;
    let Inst{23-16} = 0xFD;
    let Inst{15-14} = 0b10;
    let Inst{13-11} = Op;
    let Inst{10-8} = 0b110;
    let Inst{7-0} = d;
  }
  }
  }
}
defm ADD : IndexALU<0b000, "add", "a, ">;
defm SUB : IndexALU<0b010, "sub", "">;
defm AND : IndexALU<0b100, "and", "">;
defm XOR : IndexALU<0b101, "xor", "">;
defm OR  : IndexALU<0b110, "or", "">;
let Uses = [F] in {
defm ADC : IndexALU<0b001, "adc", "a, ">;
defm SBC : IndexALU<0b011, "sbc", "a, ">;
}
let Defs = [F], mayLoad = 1, hasSideEffects = 0 in {
def CPIXd : Z80Inst<3, (outs), (ins ACC:$a, IXR:$xy, disp8:$d),
                    "cp\t($xy$d)"> {
  bits<8> d;
  let Inst{23-8} = 0xDDBE;
  let Inst{7-0} = d;
}
def CPIYd : Z80Inst<3, (outs), (ins ACC:$a, IYR:$xy, disp8:$d),
                    "cp\t($xy$d)"> {
  bits<8> d;
  let Inst{23-8} = 0xFDBE;
  let Inst{7-0} = d;
}
}

} // Predicates = [HasZ80]

//===----------------------------------------------------------------------===//
// Pseudo instructions
//===----------------------------------------------------------------------===//

let Defs = [SP], Uses = [SP], hasSideEffects = 0 in {
def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i16imm:$amt1, i16imm:$amt2),
                              "#ADJCALLSTACKDOWN",
                              [(Z80callseq_start timm:$amt1, timm:$amt2)]>;
def ADJCALLSTACKUP : Pseudo<(outs), (ins i16imm:$amt1, i16imm:$amt2),
                            "#ADJCALLSTACKUP",
                            [(Z80callseq_end timm:$amt1, timm:$amt2)]>;
// SP += amt, after a call. Z80ExpandPseudo expands it with POPs into a free
// register pair or with ADD HL,SP.
def SPADJ : Pseudo<(outs), (ins i16imm:$amt), "#SPADJ">;
}

// Loads and stores of stack objects, and their addresses. They change no
// other register or flag: Z80ExpandPseudo saves what the expansion needs.
let hasSideEffects = 0 in {
let mayLoad = 1 in {
def LD8fi : Pseudo<(outs GR8:$dst), (ins memfi:$addr), "#LD8fi $dst, $addr",
                   [(set i8:$dst, (load addrFI:$addr))]>;
def LD16fi : Pseudo<(outs GR16:$dst), (ins memfi:$addr),
                    "#LD16fi $dst, $addr",
                    [(set i16:$dst, (load addrFI:$addr))]>;
}
let mayStore = 1 in {
def ST8fi : Pseudo<(outs), (ins memfi:$addr, GR8:$src), "#ST8fi $addr, $src",
                   [(store i8:$src, addrFI:$addr)]>;
def ST16fi : Pseudo<(outs), (ins memfi:$addr, GR16:$src),
                    "#ST16fi $addr, $src",
                    [(store i16:$src, addrFI:$addr)]>;
}
def LEAfi : Pseudo<(outs GR16:$dst), (ins memfi:$addr), "#LEAfi $dst, $addr",
                   [(set i16:$dst, addrFI:$addr)]>;
}

// 16-bit loads and stores through HL, which they leave unchanged.
let hasSideEffects = 0 in {
let mayLoad = 1 in
def LD16HL : Pseudo<(outs GR16:$dst), (ins HL16:$ptr), "#LD16HL $dst, $ptr",
                    [(set i16:$dst, (load HL16:$ptr))]>;
let mayStore = 1 in
def ST16HL : Pseudo<(outs), (ins HL16:$ptr, BCDE:$src), "#ST16HL $ptr, $src",
                    [(store i16:$src, HL16:$ptr)]>;
}

// 16-bit operations done a byte at a time through A.
let hasSideEffects = 0, Defs = [A, F] in {
let Constraints = "$a = $dst" in {
def SUB16 : Pseudo<(outs GR16:$dst), (ins GR16:$a, GR16:$b), "#SUB16",
                   [(set i16:$dst, (sub i16:$a, i16:$b))]>;
let isCommutable = 1 in {
def AND16 : Pseudo<(outs GR16:$dst), (ins GR16:$a, GR16:$b), "#AND16",
                   [(set i16:$dst, (and i16:$a, i16:$b))]>;
def OR16 : Pseudo<(outs GR16:$dst), (ins GR16:$a, GR16:$b), "#OR16",
                  [(set i16:$dst, (or i16:$a, i16:$b))]>;
def XOR16 : Pseudo<(outs GR16:$dst), (ins GR16:$a, GR16:$b), "#XOR16",
                   [(set i16:$dst, (xor i16:$a, i16:$b))]>;
}
let Uses = [F] in {
def ADC16 : Pseudo<(outs GR16:$dst), (ins GR16:$a, GR16:$b), "#ADC16",
                   [(set i16:$dst, (adde i16:$a, i16:$b))]>;
def SBC16 : Pseudo<(outs GR16:$dst), (ins GR16:$a, GR16:$b), "#SBC16",
                   [(set i16:$dst, (sube i16:$a, i16:$b))]>;
}
}
def CMP16 : Pseudo<(outs), (ins GR16:$a, GR16:$b), "#CMP16",
                   [(Z80cmp i16:$a, i16:$b)]>;
def TST16 : Pseudo<(outs), (ins GR16:$a), "#TST16", [(Z80tst i16:$a)]>;
let Constraints = "$src = $dst" in {
def SRL16 : Pseudo<(outs GR16:$dst), (ins GR16:$src), "#SRL16",
                   [(set i16:$dst, (Z80srl1 i16:$src))]>;
def SRA16 : Pseudo<(outs GR16:$dst), (ins GR16:$src), "#SRA16",
                   [(set i16:$dst, (Z80sra1 i16:$src))]>;
}
def SEXT16 : Pseudo<(outs GR16:$dst), (ins GR8:$src), "#SEXT16",
                    [(set i16:$dst, (sext i8:$src))]>;
}

// 8-bit shifts of A by one bit.
let hasSideEffects = 0, Defs = [F], Constraints = "$src = $dst" in {
def SRL8 : Pseudo<(outs ACC:$dst), (ins ACC:$src), "#SRL8",
                  [(set i8:$dst, (Z80srl1 i8:$src))]>;
def SRA8 : Pseudo<(outs ACC:$dst), (ins ACC:$src), "#SRA8",
                  [(set i8:$dst, (Z80sra1 i8:$src))]>;
}

let hasSideEffects = 0 in
def ZEXT16 : Pseudo<(outs GR16:$dst), (ins GR8:$src), "#ZEXT16",
                    [(set i16:$dst, (zext i8:$src))]>;

let usesCustomInserter = 1, Uses = [F], hasSideEffects = 0 in {
def SELECT8 : Pseudo<(outs GR8:$dst), (ins GR8:$t, GR8:$f, i8imm:$cc),
                     "#SELECT8",
                     [(set i8:$dst, (Z80selectcc i8:$t, i8:$f, timm:$cc))]>;
def SELECT16 : Pseudo<(outs GR16:$dst), (ins GR16:$t, GR16:$f, i8imm:$cc),
                      "#SELECT16",
                      [(set i16:$dst,
                            (Z80selectcc i16:$t, i16:$f, timm:$cc))]>;
}

//===----------------------------------------------------------------------===//
// Patterns
//===----------------------------------------------------------------------===//

def : Pat<(Z80wrapper tglobaladdr:$a), (LD16ri tglobaladdr:$a)>;
def : Pat<(Z80wrapper texternalsym:$a), (LD16ri texternalsym:$a)>;
def : Pat<(Z80wrapper tblockaddress:$a), (LD16ri tblockaddress:$a)>;
def : Pat<(Z80wrapper tjumptable:$a), (LD16ri tjumptable:$a)>;
def : Pat<(Z80wrapper tconstpool:$a), (LD16ri tconstpool:$a)>;

def : Pat<(Z80call (i16 tglobaladdr:$f)), (CALL tglobaladdr:$f)>;
def : Pat<(Z80call (i16 texternalsym:$f)), (CALL texternalsym:$f)>;

// 16-bit addition with a constant.
def : Pat<(add i16:$a, imm:$b), (ADDHLrp $a, (LD16ri imm:$b))>;
def : Pat<(add i16:$a, 2), (INC16 (INC16 $a))>;
def : Pat<(add i16:$a, -2), (DEC16 (DEC16 $a))>;
def : Pat<(addc i16:$a, i16:$b), (ADDHLrp $a, $b)>;
def : Pat<(addc i16:$a, imm:$b), (ADDHLrp $a, (LD16ri imm:$b))>;
def : Pat<(subc i16:$a, i16:$b), (SUB16 $a, $b)>;
def : Pat<(sub i16:$a, imm:$b), (SUB16 $a, (LD16ri imm:$b))>;
def : Pat<(and i16:$a, imm:$b), (AND16 $a, (LD16ri imm:$b))>;
def : Pat<(or i16:$a, imm:$b), (OR16 $a, (LD16ri imm:$b))>;
def : Pat<(xor i16:$a, imm:$b), (XOR16 $a, (LD16ri imm:$b))>;
def : Pat<(Z80cmp i16:$a, imm:$b), (CMP16 $a, (LD16ri imm:$b))>;

// 8-bit shifts left by one bit.
def : Pat<(Z80shl1 i8:$a), (ADDr $a, $a)>;
def : Pat<(Z80shl1 i16:$a), (ADDHLrp $a, $a)>;

def : Pat<(i8 (trunc i16:$a)), (EXTRACT_SUBREG $a, sub_lo)>;
def : Pat<(i8 (Z80hi i16:$a)), (EXTRACT_SUBREG $a, sub_hi)>;
def : Pat<(i16 (anyext i8:$a)),
          (INSERT_SUBREG (i16 (IMPLICIT_DEF)), $a, sub_lo)>;
def : Pat<(Z80combine i8:$lo, i8:$hi),
          (REG_SEQUENCE GR16, $lo, sub_lo, $hi, sub_hi)>;
