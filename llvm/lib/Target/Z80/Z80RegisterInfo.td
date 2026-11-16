//===-- Z80RegisterInfo.td - Z80 and 8080 registers --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The 8080 has seven 8-bit registers, three of which pairs (BC, DE, HL) are
// also 16-bit registers, plus SP and the flags. The Z80 adds IX and IY.
//
// The hardware encoding is the 3-bit register field of the 8-bit
// instructions, and the 2-bit pair field of the 16-bit ones (IX and IY take
// HL's encoding behind a DD or FD prefix).
//
//===----------------------------------------------------------------------===//

let Namespace = "Z80" in {
  def sub_lo : SubRegIndex<8>;
  def sub_hi : SubRegIndex<8, 8>;
}

class Z80Reg<bits<16> Enc, string Name, list<Register> Subs = []>
    : Register<Name> {
  let Namespace = "Z80";
  let HWEncoding = Enc;
  let SubRegs = Subs;
}

class Z80Pair<bits<16> Enc, string Name, Register Lo, Register Hi>
    : Z80Reg<Enc, Name, [Lo, Hi]> {
  let SubRegIndices = [sub_lo, sub_hi];
  let CoveredBySubRegs = 1;
}

def B : Z80Reg<0, "b">, DwarfRegNum<[0]>;
def C : Z80Reg<1, "c">, DwarfRegNum<[1]>;
def D : Z80Reg<2, "d">, DwarfRegNum<[2]>;
def E : Z80Reg<3, "e">, DwarfRegNum<[3]>;
def H : Z80Reg<4, "h">, DwarfRegNum<[4]>;
def L : Z80Reg<5, "l">, DwarfRegNum<[5]>;
def A : Z80Reg<7, "a">, DwarfRegNum<[7]>;
// The flags register. It is only ever an implicit operand.
def F : Z80Reg<6, "f">, DwarfRegNum<[6]>;

def BC : Z80Pair<0, "bc", C, B>, DwarfRegNum<[8]>;
def DE : Z80Pair<1, "de", E, D>, DwarfRegNum<[9]>;
def HL : Z80Pair<2, "hl", L, H>, DwarfRegNum<[10]>;
def AF : Z80Pair<3, "af", F, A>, DwarfRegNum<[11]>;
def SP : Z80Reg<3, "sp">, DwarfRegNum<[12]>;
def IX : Z80Reg<2, "ix">, DwarfRegNum<[13]>;
def IY : Z80Reg<2, "iy">, DwarfRegNum<[14]>;
// The program counter, for DWARF.
def PC : Z80Reg<0, "pc">, DwarfRegNum<[15]>;

// 8-bit registers. A comes first: every 8-bit ALU operation goes through it.
def GR8 : RegisterClass<"Z80", [i8], 8, (add A, L, E, C, H, D, B)>;
def ACC : RegisterClass<"Z80", [i8], 8, (add A)>;
// The 8-bit registers that are not part of HL.
def GR8_NOHL : RegisterClass<"Z80", [i8], 8, (add A, E, C, D, B)>;

// 16-bit register pairs.
def GR16 : RegisterClass<"Z80", [i16], 8, (add HL, DE, BC)>;
def HL16 : RegisterClass<"Z80", [i16], 8, (add HL)>;
def DE16 : RegisterClass<"Z80", [i16], 8, (add DE)>;
def BC16 : RegisterClass<"Z80", [i16], 8, (add BC)>;
// The pairs LD A,(rp) and LD (rp),A take, and the 16-bit loads and stores
// through HL use.
def BCDE : RegisterClass<"Z80", [i16], 8, (add DE, BC)>;

// Registers that are only used explicitly.
def SPR : RegisterClass<"Z80", [i16], 8, (add SP)> { let isAllocatable = 0; }
def AFR : RegisterClass<"Z80", [i16], 8, (add AF)> { let isAllocatable = 0; }
def IXR : RegisterClass<"Z80", [i16], 8, (add IX)> { let isAllocatable = 0; }
def IYR : RegisterClass<"Z80", [i16], 8, (add IY)> { let isAllocatable = 0; }
def IXY : RegisterClass<"Z80", [i16], 8, (add IX, IY)> { let isAllocatable = 0; }
def FLAGS : RegisterClass<"Z80", [i8], 8, (add F)> {
  let isAllocatable = 0;
  let CopyCost = -1;
}
