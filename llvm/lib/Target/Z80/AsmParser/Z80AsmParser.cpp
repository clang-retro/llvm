//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Parses Zilog-syntax Z80 and 8080 assembly. Memory operands are separate
// tokens: "(hl)" is "(", hl, ")", and "(ix-2)" is "(", ix, "+", -2, ")".
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "TargetInfo/Z80TargetInfo.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include <deque>

#define DEBUG_TYPE "z80-asm-parser"

using namespace llvm;

namespace {

class Z80Operand : public MCParsedAsmOperand {
  enum KindTy { k_Tok, k_Reg, k_Imm, k_Cond } Kind;
  StringRef Tok;
  MCRegister Reg;
  const MCExpr *Imm = nullptr;
  unsigned Cond = 0;
  SMLoc Start, End;

public:
  Z80Operand(KindTy K, SMLoc S, SMLoc E) : Kind(K), Start(S), End(E) {}

  static std::unique_ptr<Z80Operand> createToken(StringRef Str, SMLoc S) {
    auto Op = std::make_unique<Z80Operand>(k_Tok, S, S);
    Op->Tok = Str;
    return Op;
  }
  static std::unique_ptr<Z80Operand> createReg(MCRegister Reg, SMLoc S,
                                               SMLoc E) {
    auto Op = std::make_unique<Z80Operand>(k_Reg, S, E);
    Op->Reg = Reg;
    return Op;
  }
  static std::unique_ptr<Z80Operand> createImm(const MCExpr *Val, SMLoc S,
                                               SMLoc E) {
    auto Op = std::make_unique<Z80Operand>(k_Imm, S, E);
    Op->Imm = Val;
    return Op;
  }
  static std::unique_ptr<Z80Operand> createCond(unsigned CC, SMLoc S,
                                                SMLoc E) {
    auto Op = std::make_unique<Z80Operand>(k_Cond, S, E);
    Op->Cond = CC;
    return Op;
  }

  bool isToken() const override { return Kind == k_Tok; }
  bool isReg() const override { return Kind == k_Reg; }
  bool isImm() const override { return Kind == k_Imm; }
  bool isMem() const override { return false; }

  // C is both a register and a condition.
  bool isCondCode() const {
    return Kind == k_Cond || (Kind == k_Reg && Reg == Z80::C);
  }
  bool isJRCondCode() const {
    return (Kind == k_Cond && Cond <= Z80::COND_C) ||
           (Kind == k_Reg && Reg == Z80::C);
  }
  unsigned getCond() const { return Kind == k_Cond ? Cond : Z80::COND_C; }

  StringRef getToken() const {
    assert(Kind == k_Tok && "not a token");
    return Tok;
  }
  MCRegister getReg() const override {
    assert(Kind == k_Reg && "not a register");
    return Reg;
  }
  const MCExpr *getImm() const { return Imm; }

  SMLoc getStartLoc() const override { return Start; }
  SMLoc getEndLoc() const override { return End; }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    Inst.addOperand(MCOperand::createReg(getReg()));
  }
  void addImmOperands(MCInst &Inst, unsigned N) const {
    if (const auto *CE = dyn_cast<MCConstantExpr>(Imm))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(Imm));
  }
  void addCondCodeOperands(MCInst &Inst, unsigned N) const {
    Inst.addOperand(MCOperand::createImm(getCond()));
  }
  void addJRCondCodeOperands(MCInst &Inst, unsigned N) const {
    addCondCodeOperands(Inst, N);
  }

  void print(raw_ostream &O, const MCAsmInfo &MAI) const override {
    switch (Kind) {
    case k_Tok:
      O << "Token " << Tok;
      break;
    case k_Reg:
      O << "Register " << Reg.id();
      break;
    case k_Imm:
      O << "Immediate ";
      MAI.printExpr(O, *Imm);
      break;
    case k_Cond:
      O << "Condition " << Cond;
      break;
    }
  }
};

class Z80AsmParser : public MCTargetAsmParser {
  MCAsmParser &Parser;
  std::deque<std::string> Mnemonics;

#define GET_ASSEMBLER_HEADER
#include "Z80GenAsmMatcher.inc"

  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;
  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) override;
  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;
  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;
  unsigned validateTargetOperandClass(MCParsedAsmOperand &Op,
                                      unsigned Kind) override;

  bool parseOperand(OperandVector &Operands, bool AllowCond);

  AsmLexer &getLexer() const { return Parser.getLexer(); }

public:
  Z80AsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
               const MCInstrInfo &MII)
      : MCTargetAsmParser(STI, MII), Parser(Parser) {
    MCAsmParserExtension::Initialize(Parser);
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }
};

} // namespace

static MCRegister matchZ80RegisterName(StringRef Name) {
  return StringSwitch<MCRegister>(Name.lower())
      .Case("a", Z80::A)
      .Case("b", Z80::B)
      .Case("c", Z80::C)
      .Case("d", Z80::D)
      .Case("e", Z80::E)
      .Case("h", Z80::H)
      .Case("l", Z80::L)
      .Case("bc", Z80::BC)
      .Case("de", Z80::DE)
      .Case("hl", Z80::HL)
      .Case("sp", Z80::SP)
      .Case("af", Z80::AF)
      .Case("ix", Z80::IX)
      .Case("iy", Z80::IY)
      .Default(MCRegister());
}

static unsigned matchCondName(StringRef Name) {
  return StringSwitch<unsigned>(Name.lower())
      .Case("nz", Z80::COND_NZ)
      .Case("z", Z80::COND_Z)
      .Case("nc", Z80::COND_NC)
      .Case("c", Z80::COND_C)
      .Case("po", Z80::COND_PO)
      .Case("pe", Z80::COND_PE)
      .Case("p", Z80::COND_P)
      .Case("m", Z80::COND_M)
      .Default(Z80::COND_INVALID);
}

bool Z80AsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                 SMLoc &EndLoc) {
  if (!tryParseRegister(Reg, StartLoc, EndLoc).isSuccess())
    return Error(StartLoc, "invalid register name");
  return false;
}

ParseStatus Z80AsmParser::tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                           SMLoc &EndLoc) {
  const AsmToken &Tok = Parser.getTok();
  StartLoc = Tok.getLoc();
  EndLoc = Tok.getEndLoc();
  if (Tok.isNot(AsmToken::Identifier))
    return ParseStatus::NoMatch;
  Reg = matchZ80RegisterName(Tok.getIdentifier());
  if (!Reg)
    return ParseStatus::NoMatch;
  Parser.Lex();
  return ParseStatus::Success;
}

bool Z80AsmParser::parseOperand(OperandVector &Operands, bool AllowCond) {
  const AsmToken &Tok = Parser.getTok();
  SMLoc S = Tok.getLoc();

  if (Tok.is(AsmToken::Identifier)) {
    StringRef Id = Tok.getIdentifier();
    if (AllowCond) {
      unsigned CC = matchCondName(Id);
      if (CC != Z80::COND_INVALID) {
        SMLoc E = Tok.getEndLoc();
        Parser.Lex();
        Operands.push_back(Z80Operand::createCond(CC, S, E));
        return false;
      }
    }
    if (MCRegister Reg = matchZ80RegisterName(Id)) {
      SMLoc E = Tok.getEndLoc();
      Parser.Lex();
      Operands.push_back(Z80Operand::createReg(Reg, S, E));
      return false;
    }
  }

  if (Tok.is(AsmToken::LParen)) {
    Parser.Lex();
    Operands.push_back(Z80Operand::createToken("(", S));
    const AsmToken &Inner = Parser.getTok();
    MCRegister Reg;
    if (Inner.is(AsmToken::Identifier))
      Reg = matchZ80RegisterName(Inner.getIdentifier());
    if (Reg) {
      SMLoc RS = Inner.getLoc(), RE = Inner.getEndLoc();
      Parser.Lex();
      Operands.push_back(Z80Operand::createReg(Reg, RS, RE));
      if (Reg == Z80::IX || Reg == Z80::IY) {
        // (ix+d), (ix-d) or (ix).
        SMLoc DS = Parser.getTok().getLoc();
        const MCExpr *Disp = MCConstantExpr::create(0, getContext());
        if (Parser.getTok().is(AsmToken::Plus) ||
            Parser.getTok().is(AsmToken::Minus)) {
          SMLoc E;
          if (Parser.parseExpression(Disp, E))
            return true;
        }
        Operands.push_back(
            Z80Operand::createImm(Disp, DS, Parser.getTok().getLoc()));
      }
    } else {
      const MCExpr *Val;
      SMLoc E;
      if (Parser.parseExpression(Val, E))
        return true;
      Operands.push_back(Z80Operand::createImm(Val, Inner.getLoc(), E));
    }
    if (Parser.getTok().isNot(AsmToken::RParen))
      return Error(Parser.getTok().getLoc(), "expected ')'");
    Operands.push_back(
        Z80Operand::createToken(")", Parser.getTok().getLoc()));
    Parser.Lex();
    return false;
  }

  const MCExpr *Val;
  SMLoc E;
  if (Parser.parseExpression(Val, E))
    return true;
  Operands.push_back(Z80Operand::createImm(Val, S, E));
  return false;
}

bool Z80AsmParser::parseInstruction(ParseInstructionInfo &Info,
                                    StringRef Name, SMLoc NameLoc,
                                    OperandVector &Operands) {
  Mnemonics.push_back(Name.lower());
  StringRef Mnemonic = Mnemonics.back();
  Operands.push_back(Z80Operand::createToken(Mnemonic, NameLoc));

  // The first operand of a jump, call or return may be a condition.
  bool AllowCond = Mnemonic == "jp" || Mnemonic == "jr" ||
                   Mnemonic == "call" || Mnemonic == "ret";
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    if (parseOperand(Operands, AllowCond))
      return true;
    while (parseOptionalToken(AsmToken::Comma))
      if (parseOperand(Operands, false))
        return true;
  }
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    SMLoc Loc = getLexer().getLoc();
    Parser.eatToEndOfStatement();
    return Error(Loc, "unexpected token in operand list");
  }
  Parser.Lex();
  return false;
}

bool Z80AsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                           OperandVector &Operands,
                                           MCStreamer &Out,
                                           uint64_t &ErrorInfo,
                                           bool MatchingInlineAsm) {
  MCInst Inst;
  switch (MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm)) {
  case Match_Success:
    Inst.setLoc(IDLoc);
    Out.emitInstruction(Inst, getSTI());
    return false;
  case Match_MissingFeature:
    return Error(IDLoc, "instruction requires a Z80");
  case Match_MnemonicFail:
    return Error(IDLoc, "invalid instruction mnemonic");
  case Match_InvalidOperand: {
    SMLoc ErrorLoc = IDLoc;
    if (ErrorInfo != ~0ULL) {
      if (ErrorInfo >= Operands.size())
        return Error(IDLoc, "too few operands for instruction");
      ErrorLoc = Operands[ErrorInfo]->getStartLoc();
      if (ErrorLoc == SMLoc())
        ErrorLoc = IDLoc;
    }
    return Error(ErrorLoc, "invalid operand for instruction");
  }
  default:
    return Error(IDLoc, "invalid instruction");
  }
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeZ80AsmParser() {
  RegisterMCAsmParser<Z80AsmParser> X(getTheZ80Target());
  RegisterMCAsmParser<Z80AsmParser> Y(getTheI8080Target());
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "Z80GenAsmMatcher.inc"

// A register written literally in an instruction's syntax, like the HL of
// "ex de, hl", is a token there.
unsigned Z80AsmParser::validateTargetOperandClass(MCParsedAsmOperand &AsmOp,
                                                  unsigned Kind) {
  Z80Operand &Op = static_cast<Z80Operand &>(AsmOp);
  if (!Op.isReg())
    return Match_InvalidOperand;
  std::string Name = StringRef(getContext().getRegisterInfo()->getName(
                                   Op.getReg()))
                         .lower();
  if (matchTokenString(Name) == MatchClassKind(Kind))
    return Match_Success;
  return Match_InvalidOperand;
}
