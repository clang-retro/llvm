//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//
// The traceback table at the end of each function in XCOFF objects. AIX
// declares it in <sys/debug.h>. Other systems that use XCOFF objects (classic
// Mac OS on PowerPC) do not have that header, so the declarations that
// libunwind uses are here, with the layout of AIX.
//===----------------------------------------------------------------------===//

#ifndef __XCOFF_TBTABLE_H__
#define __XCOFF_TBTABLE_H__

#if defined(_AIX)
#include <sys/debug.h>
#else

/// The fixed part of a traceback table: 8 bytes after the zero word that
/// ends the code of the function. The bit fields are in the order of the
/// AIX header, most significant bit first.
struct tbtable_short {
  unsigned char version;   ///< Traceback format version.
  unsigned char lang;      ///< Source language (TB_CPLUSPLUS, ...).
  unsigned globallink : 1; ///< Out-of-line glue code.
  unsigned is_eprol : 1;   ///< Out-of-line prologue or epilogue.
  unsigned has_tboff : 1;  ///< The tb_offset field is present.
  unsigned int_proc : 1;   ///< Internal procedure.
  unsigned has_ctl : 1;    ///< Controlled storage information is present.
  unsigned tocless : 1;    ///< The function does not use the TOC.
  unsigned fp_present : 1; ///< The function uses floating point.
  unsigned log_abort : 1;  ///< Log or abort on exit.
  unsigned int_hndl : 1;   ///< Interrupt handler.
  unsigned name_present : 1; ///< The name of the function is present.
  unsigned uses_alloca : 1;  ///< The alloca register field is present.
  unsigned cl_dis_inv : 3;   ///< On condition actions.
  unsigned saves_cr : 1;     ///< The condition register is saved.
  unsigned saves_lr : 1;     ///< The link register is saved.
  unsigned stores_bc : 1;    ///< The back chain is stored.
  unsigned fixup : 1;        ///< Fixup code.
  unsigned fpr_saved : 6;    ///< Number of floating-point registers saved.
  unsigned longtbtable : 1;  ///< The extension table is present.
  unsigned has_vec : 1;      ///< Vector information is present.
  unsigned gpr_saved : 6;    ///< Number of general registers saved.
  unsigned fixedparms : 8;   ///< Number of fixed-point parameters.
  unsigned floatparms : 7;   ///< Number of floating-point parameters.
  unsigned parmsonstk : 1;   ///< The parameters are on the stack.
};

/// The optional fields that follow the fixed part. Only the start is a
/// real structure; the code steps over the fields that are present.
struct tbtable_ext {
  unsigned int parminfo;  ///< Parameter types (if there are parameters).
  unsigned int tb_offset; ///< Offset from the start of the function.
};

/// The vector extension of a traceback table (if has_vec is set).
struct vec_ext {
  unsigned vr_saved : 6;      ///< Number of vector registers saved.
  unsigned saves_vrsave : 1;  ///< The VRSAVE register is saved.
  unsigned has_varargs : 1;   ///< The function has variable arguments.
  unsigned vectorparms : 7;   ///< Number of vector parameters.
  unsigned vec_present : 1;   ///< The function uses vector instructions.
  unsigned char vecparminfo[4]; ///< Vector parameter types.
};

/// A traceback table.
struct tbtable {
  struct tbtable_short tb;
  struct tbtable_ext tb_ext;
};

/// The value of the lang field for C++.
#define TB_CPLUSPLUS 9

#endif // defined(_AIX)
#endif // __XCOFF_TBTABLE_H__
