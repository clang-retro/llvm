//===- llvm-exe2cmd.cpp - Make a CP/M-86 CMD file from a DOS program ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Turns a program linked as a DOS .COM or .EXE file into a CP/M-86 command
// file (.CMD): a 128-byte header of group descriptors, then the image of
// each group.
//
// A .COM file (the tiny memory model, linked at offset 100h) becomes an
// 8080-model program: one code group in which CS = DS = ES, with the base
// page at offset 0 and the program at 100h, as in the .COM file. Its image
// holds the 256 bytes of the base page, which the loader fills in. When the
// startup code begins with a short jump over "CP/M" and a word (at offsets
// 102h and 106h), the word is the end of the memory the program needs: its
// BSS and the least stack. The group is then allocated at least that much,
// and up to 64 KiB.
//
// An .EXE file (the small model) becomes a code group and a data group.
// The program starts at CS:0000 with the same marker, whose word is the
// paragraph of DGROUP: the data group, which holds the stack in the small
// model. That word is the program's one segment relocation, which the
// converter drops, since CP/M-86 sets DS itself. The data group's first 256
// bytes are the base page. Its minimum size covers the
// uninitialized data after the image (the .EXE header's minimum
// allocation), and it can grow to 64 KiB. CP/M-86 1.1 cannot fix up segment
// addresses, so an .EXE file with any other relocation is refused.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cstring>

using namespace llvm;
using namespace llvm::support;

static cl::OptionCategory Cat("llvm-exe2cmd options");
static cl::opt<std::string> InputFile(cl::Positional, cl::Required,
                                      cl::desc("<input .COM or .EXE>"),
                                      cl::cat(Cat));
static cl::opt<std::string> OutputFile("o", cl::Required,
                                       cl::desc("Output CMD file"),
                                       cl::value_desc("file"), cl::cat(Cat));
static cl::opt<unsigned>
    MaxParas("max",
             cl::desc("Most paragraphs the data group (the code group of an "
                      "8080-model program) may be given (default 4096, 64 "
                      "KiB)"),
             cl::value_desc("paragraphs"), cl::init(0x1000), cl::cat(Cat));
static cl::opt<bool> Verbose("v", cl::desc("Print the groups"), cl::cat(Cat));

static StringRef ToolName;

[[noreturn]] static void fail(const Twine &Msg) {
  WithColor::error(errs(), ToolName) << Msg << "\n";
  exit(1);
}

namespace {
struct Group {
  uint8_t Type; // 1 code, 2 data
  ArrayRef<uint8_t> Image;
  uint16_t Length; // paragraphs in the file
  uint16_t Min, Max;
};
} // namespace

static uint16_t paras(uint32_t Bytes) {
  uint32_t P = (Bytes + 15) / 16;
  if (P > 0x1000)
    fail("a group of " + Twine(Bytes) + " bytes is larger than 64 KiB");
  return P;
}

static std::vector<uint8_t> BasePageAndCom;

static SmallVector<Group, 2> fromCom(ArrayRef<uint8_t> Com) {
  if (Com.size() > 0x10000 - 0x100)
    fail("the .COM file is larger than 64 KiB less the base page");
  BasePageAndCom.assign(0x100, 0);
  BasePageAndCom.insert(BasePageAndCom.end(), Com.begin(), Com.end());
  Group G;
  G.Type = 1;
  G.Image = BasePageAndCom;
  G.Length = paras(BasePageAndCom.size());
  uint32_t Need = BasePageAndCom.size();
  if (Com.size() >= 8 && Com[0] == 0xeb && Com[1] >= 6 &&
      std::equal(Com.begin() + 2, Com.begin() + 6, "CP/M")) {
    uint16_t End = endian::read16le(Com.data() + 6);
    // 0 is the end of the segment: the program needs all 64 KiB.
    Need = std::max<uint32_t>(Need, End ? End : 0x10000);
  }
  G.Min = paras(Need);
  G.Max = std::max<uint32_t>(G.Min, MaxParas);
  return {G};
}

static SmallVector<Group, 2> fromExe(ArrayRef<uint8_t> Exe) {
  if (Exe.size() < 0x1c)
    fail("the .EXE header is truncated");
  auto W = [&](unsigned Off) { return endian::read16le(Exe.data() + Off); };
  uint16_t LastPage = W(2), Pages = W(4), Relocs = W(6), HeaderParas = W(8);
  uint16_t MinAlloc = W(0xa), IP = W(0x14), CS = W(0x16);
  uint32_t ImageEnd = Pages * 512u - (LastPage ? 512 - LastPage : 0);
  uint32_t ImageStart = HeaderParas * 16u;
  if (ImageEnd > Exe.size() || ImageStart > ImageEnd)
    fail("the .EXE file is truncated");
  if (CS != 0 || IP != 0)
    fail("the program must start at the first byte of its code (it starts "
         "at " +
         utohexstr(CS, false, 4) + ":" + utohexstr(IP, false, 4) +
         "); link the startup code first");
  ArrayRef<uint8_t> Image = Exe.slice(ImageStart, ImageEnd - ImageStart);
  if (Image.size() < 8 || Image[0] != 0xeb || Image[1] != 0x06 ||
      memcmp(Image.data() + 2, "CP/M", 4) != 0)
    fail("the program doesn't start with the CP/M-86 marker; link the CP/M-86 "
         "startup code first");
  // The marker's word is the data group's paragraph, so the one segment
  // relocation the program may have is the one at 0000:0006. CP/M-86 sets
  // DS itself, so the converter drops it; any other can't be fixed up.
  uint16_t RelocOff = W(0x18);
  if (RelocOff + Relocs * 4u > Exe.size())
    fail("the .EXE relocation table is truncated");
  bool Marker = false;
  for (unsigned I = 0; I < Relocs; ++I) {
    uint16_t Off = endian::read16le(Exe.data() + RelocOff + I * 4);
    uint16_t Seg = endian::read16le(Exe.data() + RelocOff + I * 4 + 2);
    if (Seg == 0 && Off == 6 && !Marker) {
      Marker = true;
      continue;
    }
    fail("the program has a segment relocation at " + utohexstr(Seg, false, 4) +
         ":" + utohexstr(Off, false, 4) +
         ", which CP/M-86 can't fix up; use the tiny or small model and no "
         "far data");
  }
  if (!Marker)
    fail("the CP/M-86 marker doesn't hold the data group's segment; link the "
         "small model CP/M-86 startup code first");
  uint16_t DSeg = endian::read16le(Image.data() + 6);
  uint32_t DataStart = DSeg * 16u;
  if (DSeg == 0 || DataStart > Image.size())
    fail("the data group (" + utohexstr(DSeg, false, 4) +
         ") isn't inside the program");
  Group Code;
  Code.Type = 1;
  Code.Image = Image.take_front(DataStart);
  Code.Length = paras(Code.Image.size());
  Code.Min = Code.Max = Code.Length;
  Group Data;
  Data.Type = 2;
  Data.Image = Image.drop_front(DataStart);
  if (Data.Image.size() < 0x100)
    fail("the data group has no room for the base page; link the CP/M-86 "
         "startup code first");
  Data.Length = paras(Data.Image.size());
  Data.Min = paras(Data.Length * 16u + MinAlloc * 16u);
  Data.Max = std::max<uint32_t>(Data.Min, MaxParas);
  return {Code, Data};
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);
  ToolName = argv[0];
  cl::HideUnrelatedOptions(Cat);
  cl::ParseCommandLineOptions(
      argc, argv, "Make a CP/M-86 CMD file from a DOS .COM or .EXE file\n");
  if (MaxParas == 0 || MaxParas > 0xffff)
    fail("-max must be from 1 to ffff paragraphs");

  ErrorOr<std::unique_ptr<MemoryBuffer>> Buf =
      MemoryBuffer::getFile(InputFile, /*IsText=*/false,
                            /*RequiresNullTerminator=*/false);
  if (!Buf)
    fail(InputFile + ": " + Buf.getError().message());
  ArrayRef<uint8_t> In(
      reinterpret_cast<const uint8_t *>((*Buf)->getBufferStart()),
      (*Buf)->getBufferSize());
  bool IsExe = In.size() >= 2 && ((In[0] == 'M' && In[1] == 'Z') ||
                                  (In[0] == 'Z' && In[1] == 'M'));
  SmallVector<Group, 2> Groups = IsExe ? fromExe(In) : fromCom(In);

  size_t Size = 128;
  for (const Group &G : Groups)
    Size += G.Length * 16u;
  Expected<std::unique_ptr<FileOutputBuffer>> Out =
      FileOutputBuffer::create(OutputFile, Size);
  if (!Out)
    fail(OutputFile + ": " + toString(Out.takeError()));
  uint8_t *P = (*Out)->getBufferStart();
  std::fill(P, P + Size, 0);
  uint8_t *D = P;
  uint8_t *Img = P + 128;
  for (const Group &G : Groups) {
    D[0] = G.Type;
    endian::write16le(D + 1, G.Length);
    endian::write16le(D + 3, 0); // relocatable
    endian::write16le(D + 5, G.Min);
    endian::write16le(D + 7, G.Max);
    D += 9;
    std::copy(G.Image.begin(), G.Image.end(), Img);
    Img += G.Length * 16u;
    if (Verbose)
      outs() << (G.Type == 1 ? "code" : "data") << " group: " << G.Image.size()
             << " bytes, min " << utohexstr(G.Min) << "h max "
             << utohexstr(G.Max) << "h paragraphs\n";
  }
  if (Verbose)
    outs() << (Groups.size() == 1 ? "8080" : "small") << " model\n";
  if (Error E = (*Out)->commit())
    fail(OutputFile + ": " + toString(std::move(E)));
  return 0;
}
