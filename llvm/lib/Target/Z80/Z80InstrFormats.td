//===-- Z80InstrFormats.td - Z80 instruction formats -------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// An instruction is 1 to 4 bytes. Inst holds them most significant byte
// first: the code emitter writes byte k of an n-byte instruction from
// Inst{8*(n-k)-1 .. 8*(n-k-1)}. A 16-bit immediate is stored little endian,
// so its low byte goes in the higher bits of Inst.
//
//===----------------------------------------------------------------------===//

class Z80Inst<int Sz, dag outs, dag ins, string asmstr, list<dag> pattern = []>
    : Instruction {
  let Namespace = "Z80";
  let Size = Sz;
  field bits<32> Inst = 0;
  field bits<32> SoftFail = 0;
  let OutOperandList = outs;
  let InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;
}

// An instruction without operand fields: Sz bytes of Opc.
class I<int Sz, bits<32> Opc, dag outs, dag ins, string asmstr,
        list<dag> pattern = []>
    : Z80Inst<Sz, outs, ins, asmstr, pattern> {
  let Inst = Opc;
}

// An instruction whose last byte is an 8-bit immediate.
class In<int Sz, bits<24> Opc, dag outs, dag ins, string asmstr,
         list<dag> pattern = []>
    : Z80Inst<Sz, outs, ins, asmstr, pattern> {
  bits<8> n;
  let Inst{31-8} = Opc;
  let Inst{7-0} = n;
}

// An instruction whose last two bytes are a 16-bit immediate.
class Inn<int Sz, bits<16> Opc, dag outs, dag ins, string asmstr,
          list<dag> pattern = []>
    : Z80Inst<Sz, outs, ins, asmstr, pattern> {
  bits<16> nn;
  let Inst{31-16} = Opc;
  let Inst{15-8} = nn{7-0};
  let Inst{7-0} = nn{15-8};
}

// A pseudo instruction, expanded before emission.
class Pseudo<dag outs, dag ins, string asmstr, list<dag> pattern = []>
    : Z80Inst<0, outs, ins, asmstr, pattern> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}
