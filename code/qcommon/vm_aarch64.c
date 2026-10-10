/*
===========================================================================
Copyright (C) 2009 David S. Miller <davem@davemloft.net>
Copyright (C) 2013,2014 SUSE Linux Products GmbH
Copyright (C) 2020-2026 Quake3e project

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================

AArch64 (ARM64) VM JIT for ioquake3-compatible codebases (sof2plus).
Quake3e's optimizer framework (vm_optimize.h, register allocator,
   VM_LoadInstructions, absolute-address instructionPointers, rtChecks
   cvars) is intentionally NOT used: this JIT emits simple stack-machine
   code like the armv7 one, with full 64-bit addresses (no 32-bit
   address space assumption).

Register map (x19-x24 are callee-saved, so C calls are safe):
	x19	rOPSTACK	VM opstack top pointer (grows up)
	x20	rOPSTACKBASE	VM opstack base (debug/sentinel)
	x21	rCODEBASE	host code base (vm->codeBase)
	w22	rPSTACK		VM programStack (byte offset into dataBase)
	x23	rDATABASE	host data base (vm->dataBase)
	w24	rDATAMASK	vm->dataMask
	x0-x3, x12	scratch (caller-saved, freely clobbered)
	s0, s1		float scratch
*/

#ifdef __aarch64__

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "vm_local.h"

/* macOS on Apple Silicon needs MAP_JIT + write-protect toggling */
#ifdef __APPLE__
#ifndef MAP_JIT
#define MAP_JIT 0x0800
#endif
extern void pthread_jit_write_protect_np(int) __attribute__((weak_import));
static void jit_protect(int on)
{
	if (pthread_jit_write_protect_np)
		pthread_jit_write_protect_np(on);
}
#define MMAP_FLAGS (MAP_PRIVATE|MAP_ANONYMOUS|MAP_JIT)
#else
static void jit_protect(int on) { (void)on; }
#define MMAP_FLAGS (MAP_SHARED|MAP_ANONYMOUS)
#endif

#define rOPSTACK	19
#define rOPSTACKBASE	20
#define rCODEBASE	21
#define rPSTACK		22	/* accessed as W22 */
#define rDATABASE	23
#define rDATAMASK	24	/* accessed as W24 */

#define rTMP0		0
#define rTMP1		1
#define rTMP2		2
#define rTMP3		3
#define rIP		12

#define sTMP0		0
#define sTMP1		1

#define LR		30
#define SP		31

/* exit() won't be called but use it because it is marked with noreturn */
#define DIE( reason, args... ) \
	do { \
		Com_Error(ERR_DROP, "vm_aarch64 compiler error: " reason, ##args); \
		exit(1); \
	} while(0)

/*
 * opcode information table:
 * - length of immediate value
 * - returned register type
 * - required register(s) type
 */
#define opImm0	0x0000 /* no immediate */
#define opImm1	0x0001 /* 1 byte immadiate value after opcode */
#define opImm4	0x0002 /* 4 bytes immediate value after opcode */

#define opRet0	0x0000 /* returns nothing */
#define opRetI	0x0004 /* returns integer */
#define opRetF	0x0008 /* returns float */
#define opRetIF	(opRetI | opRetF) /* returns integer or float */

#define opArg0	0x0000 /* requires nothing */
#define opArgI	0x0010 /* requires integer(s) */
#define opArgF	0x0020 /* requires float(s) */
#define opArgIF	(opArgI | opArgF) /* requires integer or float */

#define opArg2I	0x0040 /* requires second argument, integer */
#define opArg2F	0x0080 /* requires second argument, float */
#define opArg2IF (opArg2I | opArg2F) /* requires second argument, integer or float */

static const unsigned char vm_opInfo[256] =
{
	[OP_UNDEF]	= opImm0,
	[OP_IGNORE]	= opImm0,
	[OP_BREAK]	= opImm0,
	[OP_ENTER]	= opImm4,
			/* OP_LEAVE has to accept floats, they will be converted to ints */
	[OP_LEAVE]	= opImm4 | opRet0 | opArgIF,
			/* only STORE4 and POP use values from OP_CALL,
			 * no need to convert floats back */
	[OP_CALL]	= opImm0 | opRetI | opArgI,
	[OP_PUSH]	= opImm0 | opRetIF,
	[OP_POP]	= opImm0 | opRet0 | opArgIF,
	[OP_CONST]	= opImm4 | opRetIF,
	[OP_LOCAL]	= opImm4 | opRetI,
	[OP_JUMP]	= opImm0 | opRet0 | opArgI,

	[OP_EQ]		= opImm4 | opRet0 | opArgI | opArg2I,
	[OP_NE]		= opImm4 | opRet0 | opArgI | opArg2I,
	[OP_LTI]	= opImm4 | opRet0 | opArgI | opArg2I,
	[OP_LEI]	= opImm4 | opRet0 | opArgI | opArg2I,
	[OP_GTI]	= opImm4 | opRet0 | opArgI | opArg2I,
	[OP_GEI]	= opImm4 | opRet0 | opArgI | opArg2I,
	[OP_LTU]	= opImm4 | opRet0 | opArgI | opArg2I,
	[OP_LEU]	= opImm4 | opRet0 | opArgI | opArg2I,
	[OP_GTU]	= opImm4 | opRet0 | opArgI | opArg2I,
	[OP_GEU]	= opImm4 | opRet0 | opArgI | opArg2I,
	[OP_EQF]	= opImm4 | opRet0 | opArgF | opArg2F,
	[OP_NEF]	= opImm4 | opRet0 | opArgF | opArg2F,
	[OP_LTF]	= opImm4 | opRet0 | opArgF | opArg2F,
	[OP_LEF]	= opImm4 | opRet0 | opArgF | opArg2F,
	[OP_GTF]	= opImm4 | opRet0 | opArgF | opArg2F,
	[OP_GEF]	= opImm4 | opRet0 | opArgF | opArg2F,

	[OP_LOAD1]	= opImm0 | opRetI | opArgI,
	[OP_LOAD2]	= opImm0 | opRetI | opArgI,
	[OP_LOAD4]	= opImm0 | opRetIF| opArgI,
	[OP_STORE1]	= opImm0 | opRet0 | opArgI | opArg2I,
	[OP_STORE2]	= opImm0 | opRet0 | opArgI | opArg2I,
	[OP_STORE4]	= opImm0 | opRet0 | opArgIF| opArg2I,
	[OP_ARG]	= opImm1 | opRet0 | opArgIF,
	[OP_BLOCK_COPY]	= opImm4 | opRet0 | opArgI | opArg2I,

	[OP_SEX8]	= opImm0 | opRetI | opArgI,
	[OP_SEX16]	= opImm0 | opRetI | opArgI,
	[OP_NEGI]	= opImm0 | opRetI | opArgI,
	[OP_ADD]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_SUB]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_DIVI]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_DIVU]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_MODI]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_MODU]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_MULI]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_MULU]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_BAND]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_BOR]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_BXOR]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_BCOM]	= opImm0 | opRetI | opArgI,
	[OP_LSH]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_RSHI]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_RSHU]	= opImm0 | opRetI | opArgI | opArg2I,
	[OP_NEGF]	= opImm0 | opRetF | opArgF,
	[OP_ADDF]	= opImm0 | opRetF | opArgF | opArg2F,
	[OP_SUBF]	= opImm0 | opRetF | opArgF | opArg2F,
	[OP_DIVF]	= opImm0 | opRetF | opArgF | opArg2F,
	[OP_MULF]	= opImm0 | opRetF | opArgF | opArg2F,
	[OP_CVIF]	= opImm0 | opRetF | opArgI,
	[OP_CVFI]	= opImm0 | opRetI | opArgF,
};

#ifdef DEBUG_VM
static const char *opnames[256] = {
	"OP_UNDEF", "OP_IGNORE", "OP_BREAK", "OP_ENTER", "OP_LEAVE", "OP_CALL",
	"OP_PUSH", "OP_POP", "OP_CONST", "OP_LOCAL", "OP_JUMP",
	"OP_EQ", "OP_NE", "OP_LTI", "OP_LEI", "OP_GTI", "OP_GEI",
	"OP_LTU", "OP_LEU", "OP_GTU", "OP_GEU", "OP_EQF", "OP_NEF",
	"OP_LTF", "OP_LEF", "OP_GTF", "OP_GEF",
	"OP_LOAD1", "OP_LOAD2", "OP_LOAD4", "OP_STORE1", "OP_STORE2",
	"OP_STORE4", "OP_ARG", "OP_BLOCK_COPY",
	"OP_SEX8", "OP_SEX16",
	"OP_NEGI", "OP_ADD", "OP_SUB", "OP_DIVI", "OP_DIVU",
	"OP_MODI", "OP_MODU", "OP_MULI", "OP_MULU", "OP_BAND",
	"OP_BOR", "OP_BXOR", "OP_BCOM", "OP_LSH", "OP_RSHI", "OP_RSHU",
	"OP_NEGF", "OP_ADDF", "OP_SUBF", "OP_DIVF", "OP_MULF",
	"OP_CVIF", "OP_CVFI",
};

#define NOTIMPL(x) \
	do { Com_Error(ERR_DROP, "instruction not implemented: %s", opnames[x]); } while(0)
#else
#define NOTIMPL(x) \
	do { Com_Printf(S_COLOR_RED "instruction not implemented: %x\n", x); vm->compiled = qfalse; return; } while(0)
#endif

static void VM_Destroy_Compiled(vm_t *vm)
{
	if (vm->codeBase) {
		if (munmap(vm->codeBase, vm->codeLength))
			Com_Printf(S_COLOR_RED "Memory unmap failed, possible memory leak\n");
	}
	vm->codeBase = NULL;
}

/*
=================
ErrJump
Error handler for jump/call to invalid instruction number
=================
*/

static void Q_NO_RETURN ErrJump(unsigned num)
{
	Com_Error(ERR_DROP, "program tried to execute code outside VM (%x)", num);
}

static int asmcall(int call, int pstack)
{
	// save currentVM so as to allow for recursive VM entry
	vm_t *savedVM = currentVM;
	int i, ret;

	// modify VM stack pointer for recursive VM entry
	currentVM->programStack = pstack - 4;

	if (sizeof(intptr_t) == sizeof(int)) {
		intptr_t *argPosition = (intptr_t *)((byte *)currentVM->dataBase + pstack + 4);
		argPosition[0] = -1 - call;
		ret = currentVM->systemCall(qtrue, argPosition);
	} else {
		intptr_t args[MAX_VMSYSCALL_ARGS];

		args[0] = -1 - call;
		int *argPosition = (int *)((byte *)currentVM->dataBase + pstack + 4);
		for( i = 1; i < ARRAY_LEN(args); i++ )
			args[i] = argPosition[i];

		ret = currentVM->systemCall(qtrue, args);
	}

	currentVM = savedVM;

	return ret;
}

void _emit(vm_t *vm, unsigned isn, int pass)
{
	if (pass)
		memcpy((byte *)vm->codeBase + vm->codeLength, &isn, 4);
	vm->codeLength += 4;
}

#define emit(isn) _emit(vm, isn, pass)

static void store32(vm_t *vm, unsigned off, unsigned isn)
{
	memcpy((byte *)vm->codeBase + off, &isn, 4);
}

/* branch range checks: off is target - branch_pos, in bytes.
 * Bit-compatible with the Quake3e encode_offset26/encode_offset19
 * helpers, adapted to this file's two-pass structure. */
static unsigned b_rel(int off)
{
	int w;
	if (off & 3)
		DIE("branch offset must be multiple of four (%d)", off);
	w = off >> 2;
	if (w >= (1 << 25) || w < -(1 << 25))
		DIE("branch out of range (%d)", off);
	return (unsigned)w & 0x03FFFFFFu;
}

static unsigned bc_rel(int off)
{
	int w;
	if (off & 3)
		DIE("branch offset must be multiple of four (%d)", off);
	w = off >> 2;
	if (w >= (1 << 18) || w < -(1 << 18))
		DIE("conditional branch out of range (%d)", off);
	return (unsigned)w & 0x7FFFFu;
}

static unsigned inv_cond(unsigned c)
{
	static const unsigned char t[16] =
		{ 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 14, 15 };
	return t[c & 15];
}

/* conditions */
#define EQ (0b0000) // equal/equals zero
#define NE (0b0001) // not equal
#define CS (0b0010) // unsigned higher or same
#define HS CS       // unsigned higher or same
#define CC (0b0011) // unsigned lower
#define LO CC       // unsigned lower
#define MI (0b0100) // minus/negative
#define PL (0b0101) // plus/positive or zero
#define VS (0b0110) // overflow
#define VC (0b0111) // no overflow
#define HI (0b1000) // unsigned higher
#define LS (0b1001) // unsigned lower or same
#define GE (0b1010) // signed greater or equal
#define LT (0b1011) // signed less than
#define GT (0b1100) // signed greater than
#define LE (0b1101) // signed less than or equal
#define AL (0b1110) // always

/* forward conditional branch with later patching (for small static skips) */
static unsigned emit_bcond(vm_t *vm, int pass, unsigned cond)
{
	unsigned pos = (unsigned)vm->codeLength;
	emit(pass ? ((0b0101010<<25) | (0<<24) | (0<<5) | (0<<4) | ((cond) & 0xF)) : 0);
	return pos;
}

static void fix_bcond(vm_t *vm, int pass, unsigned pos, unsigned cond)
{
	if (pass)
		store32(vm, pos, (0b0101010<<25) | (0<<24) | (bc_rel((int)vm->codeLength - (int)pos) << 5) | (0<<4) | ((cond) & 0xF));
}

/* forward branch-with-link with later patching (prologue -> code entry) */
static unsigned emit_bl(vm_t *vm, int pass)
{
	unsigned pos = (unsigned)vm->codeLength;
	emit(pass ? ((1<<31) | (0b00101<<26)) : 0);
	return pos;
}

static void fix_bl(vm_t *vm, int pass, unsigned pos)
{
	if (pass)
		store32(vm, pos, ((1<<31) | (0b00101<<26)) | b_rel((int)vm->codeLength - (int)pos));
}

/* forward unconditional branch with later patching */
static unsigned emit_b(vm_t *vm, int pass)
{
	unsigned pos = (unsigned)vm->codeLength;
	emit(pass ? ((0<<31) | (0b00101<<26)) : 0);
	return pos;
}

static void fix_b(vm_t *vm, int pass, unsigned pos)
{
	if (pass)
		store32(vm, pos, ((0<<31) | (0b00101<<26)) | b_rel((int)vm->codeLength - (int)pos));
}

/*
 * Far conditional branch to a VM target.
 * Always two instructions (B.cond-over + B) so the encoding never
 * overflows the +/-1MB B.cond range and both compiler passes emit
 * exactly the same instruction count.
 */
static void emit_far(vm_t *vm, int pass, unsigned cond, int target)
{
	if (!pass) {
		emit(0);
		emit(0);
	} else {
		unsigned pos = (unsigned)vm->codeLength;
		emit((0b0101010<<25) | (0<<24) | (bc_rel(8) << 5) | (0<<4) | (inv_cond(cond) & 0xF));
		emit(((0<<31) | (0b00101<<26)) | b_rel(target - (int)(pos + 4)));
	}
}

/* =========================================================================
 * AArch64 assembler core, taken from the Quake3e vm_aarch64.c.
 * Bit layouts are untouched; only the surrounding helpers (which in
 * Quake3e depend on its optimizer pass state) were adapted.
 * ========================================================================= */

#define WZR 0b11111
#define XZR 0b11111

#define NOP                     ( (0b1101010100<<22) | (0b000011<<16) | (0b00100000<<8) | 0b00011111 )
#define BRK(imm16)              ( (0b11010100001<<21) | (imm16<<5) )
#define RET(Rn)                 ( (0b1101011<<25) | (0b0010<<21) | (0b11111<<16) | (0b000000<<10) | (Rn<<5) | 0b00000 /*Rm*/ )

#define MOVZ32(Rd,imm16)        ( (0<<31) /*sf*/ | (0b10100101<<23) | (0b00<<21) | (((imm16)&0xFFFF)<<5) | Rd )
#define MOVZ32_16(Rd,imm16)     ( (0<<31) /*sf*/ | (0b10100101<<23) | (0b01<<21) | (((imm16)&0xFFFF)<<5) | Rd )
#define MOVZ64(Rd,imm16)        ( (1<<31) /*sf*/ | (0b10100101<<23) | (0b00<<21) | (((imm16)&0xFFFF)<<5) | Rd )

#define MOVK32_16(Rd,imm16)     ( (0<<31) /*sf*/ | (0b11100101<<23) | (0b01<<21) | (((imm16)&0xFFFF)<<5) | Rd )
#define MOVK64_16(Rd,imm16)     ( (1<<31) /*sf*/ | (0b11100101<<23) | (0b01<<21) | (((imm16)&0xFFFF)<<5) | Rd )
#define MOVK64_32(Rd,imm16)     ( (1<<31) /*sf*/ | (0b11100101<<23) | (0b10<<21) | (((imm16)&0xFFFF)<<5) | Rd )
#define MOVK64_48(Rd,imm16)     ( (1<<31) /*sf*/ | (0b11100101<<23) | (0b11<<21) | (((imm16)&0xFFFF)<<5) | Rd )

#define MOVN32(Rd,imm16)        ( (0<<31) /*sf*/ | (0b00100101<<23) | (0b00<<21) | ((imm16&0xFFFF)<<5) | Rd )

#define ORR32(Rd, Rn, Rm)       ( (0<<31) /*sf*/ | 0b0101010 << 24 | 0b00<<22 /*shift*/ | (0<<21) /*N*/ | (Rm<<16) | 0b000000<<10 /*imm6*/ | (Rn<<5) | Rd )
#define ORR64(Rd, Rn, Rm)       ( (1<<31) /*sf*/ | 0b0101010 << 24 | 0b00<<22 /*shift*/ | (0<<21) /*N*/ | (Rm<<16) | 0b000000<<10 /*imm6*/ | (Rn<<5) | Rd )

#define EOR32(Rd, Rn, Rm)       ( (0<<31) /*sf*/ | (0b1001010<<24) | 0b00<<22 /*shift*/ | (0<<21) /*N*/ | (Rm<<16) | 0b000000<<10 /*imm6*/ | (Rn<<5) | Rd )

#define AND32(Rd, Rn, Rm)       ( (0<<31) /*sf*/ | (0b0001010<<24) | 0b00<<22 /*shift*/ | (0<<21) /*N*/ | (Rm<<16) | 0b000000<<10 /*imm6*/ | (Rn<<5) | Rd )

#define ORR32i(Rd, Rn, immrs)   ( (0<<31) /*sf*/ | (0b01<<29) | (0b100100 << 23) | ((immrs) << 10) | ((Rn)<<5) | (Rd) )

#define MOV32(Rd, Rm)           ORR32(Rd, WZR, Rm)
#define MOV64(Rd, Rm)           ORR64(Rd, XZR, Rm)

#define MOV32i(Rd, immrs)       ORR32i(Rd, WZR, immrs)

// MUL, alias for MADD
#define MUL32(Rd, Rn, Rm)       ( (0<<31) | (0b00<<29) | (0b11011<<24) | (0b000<<21) | (Rm<<16) | (0<<15) | (WZR<<10) /*Ra*/ | (Rn<<5) | Rd )

// ADD (shifted register)
#define ADD32(Rd, Rn, Rm)       ( (0<<31) | (0b0001011000<<21) | (Rm<<16) | (0b000000<<10) /*imm6*/ | (Rn<<5) | Rd  )
#define ADD64(Rd, Rn, Rm)       ( (1<<31) | (0b0001011000<<21) | (Rm<<16) | (0b000000<<10) /*imm6*/ | (Rn<<5) | Rd  )

// ADD (immediate)
#define ADD32i(Rd, Rn, pimm12)  ( (0<<31) | (0b00100010<<23) | (0<<22) /*sh*/ | ((pimm12)<<10) | (Rn<<5) | Rd )
#define ADD64i(Rd, Rn, pimm12)  ( (1<<31) | (0b00100010<<23) | (0<<22) /*sh*/ | ((pimm12)<<10) | (Rn<<5) | Rd )

// SUB (shifted register)
#define SUB32(Rd, Rn, Rm)       ( (0<<31) | 0b1001011000<<21 | (Rm<<16) | 0b000000<<10 /*imm6*/ | (Rn<<5) | Rd  )

// SUB (immediate)
#define SUB32i(Rd, Rn, pimm12)  ( (0<<31) | (0b10100010<<23) | (0<<22) /*sh*/ | ((pimm12)<<10) | (Rn<<5) | Rd )
#define SUB64i(Rd, Rn, pimm12)  ( (1<<31) | (0b10100010<<23) | (0<<22) /*sh*/ | ((pimm12)<<10) | (Rn<<5) | Rd )

#define SDIV32(Rd, Rn, Rm)      ( (0<<31) | (0b00<<29) | (0b11010110<<21) | (Rm<<16) | (0b00001<<11) | (1<<10) | (Rn<<5) | Rd )
#define UDIV32(Rd, Rn, Rm)      ( (0<<31) | (0b00<<29) | (0b11010110<<21) | (Rm<<16) | (0b00001<<11) | (0<<10) | (Rn<<5) | Rd )

#define MSUB32(Rd, Rn,Rm, Ra)   ( (0<<31) | (0b00<<29) | (0b11011<<24) | (0b000<<21) | (Rm<<16) | (1<<15) | (Ra<<10) | (Rn<<5) | Rd )

// MVN, alias for ORN (shifted register)
#define MVN32(Rd, Rm)           ( (0<<31) | (0b01<<29) | (0b01010<<24) | (0b001<<21) | (Rm<<16) | (0b000000<<10) | (0b11111<<5) | Rd  )

// NEG (shifted register), alias for SUB(shifted register)
#define NEG32(Rd, Rm)           SUB32(Rd, WZR, Rm)

// LSL (register)
#define LSL32(Rd, Rn, Rm)       ( (0<<31) | (0b00<<29) | (0b11010110<<21) | (Rm<<16) | (0b0010<<12) | (0b00<<10) | (Rn<<5) | Rd )

// LSR (register)
#define LSR32(Rd, Rn, Rm)       ( (0<<31) | (0b00<<29) | (0b11010110<<21) | (Rm<<16) | (0b0010<<12) | (0b01<<10) | (Rn<<5) | Rd )

// ASR (register)
#define ASR32(Rd, Rn, Rm)       ( (0<<31) | (0b00<<29) | (0b11010110<<21) | (Rm<<16) | (0b0010<<12) | (0b10<<10) | (Rn<<5) | Rd )

// LSL (immediate in range 0..31)
#define LSL32i(Rd, Rn, shift)   ( (0<<31) | (0b10<<29) | (0b100110<<23) | (0<<22) | (((-(shift))&31)<<16) | ((31-(shift))<<10) | ((Rn)<<5) | Rd )

#define SXTB(Rd, Rn)               ( (0<<31) | (0b00<<29) | (0b100110<<23) | (0<<22) /*N*/ | (0b000000<<16) /*immr*/ | (0b000111<<10) /*imms*/ | (Rn<<5) | Rd )
#define UXTB(Rd, Rn)               ( (0<<31) | (0b10<<29) | (0b100110<<23) | (0<<22) /*N*/ | (0b000000<<16) /*immr*/ | (0b000111<<10) /*imms*/ | (Rn<<5) | Rd )
#define SXTH(Rd, Rn)               ( (0<<31) | (0b00<<29) | (0b100110<<23) | (0<<22) /*N*/ | (0b000000<<16) /*immr*/ | (0b001111<<10) /*imms*/ | (Rn<<5) | Rd )
#define UXTH(Rd, Rn)               ( (0<<31) | (0b10<<29) | (0b100110<<23) | (0<<22) /*N*/ | (0b000000<<16) /*immr*/ | (0b001111<<10) /*imms*/ | (Rn<<5) | Rd )

// CMP (immediate)
#define CMP32i(Rn, imm12)          ( (0<<31) | (0b11<<29) | (0b100010<<23) | (0<<22) /*sh*/ | (imm12) << 10 | (Rn<<5) | WZR /*Rd*/ )

// CMP (shifted register)
#define CMP32(Rn, Rm)              ( (0<<31) | (0b11<<29) | (0b01011<<24) | (0b00<<22) /*sh*/ | (0<<21) | (Rm<<16) | (0b000000<<10) /*imm6*/ | (Rn<<5) | WZR /*Rd*/ )

#define STP64(Rt1,Rt2,Rn,simm7)     ( 0b10<<30 | 0b101<<27 | 0<<26 | 0b010<<23 | 0<<22 /*L*/ | ((((simm7)>>3)&0x7F)<<15) | ((Rt2)<<10) | ((Rn)<<5) | (Rt1) )
#define LDP64(Rt1,Rt2,Rn,simm7)     ( 0b10<<30 | 0b101<<27 | 0<<26 | 0b010<<23 | 1<<22 /*L*/ | ((((simm7)>>3)&0x7F)<<15) | Rt2<<10 | Rn<<5 | Rt1 )

#define LDRB32i(Rt, Rn, imm12)  ( (0b00<<30) | (0b11100101<<22) |  (imm12_scale((imm12),0) << 10) | (Rn << 5) | Rt )
#define LDRH32i(Rt, Rn, imm12)  ( (0b01<<30) | (0b11100101<<22) |  (imm12_scale((imm12),1) << 10) | (Rn << 5) | Rt )
#define LDR32i(Rt, Rn, imm12)   ( (0b10<<30) | (0b11100101<<22) |  (imm12_scale((imm12),2) << 10) | (Rn << 5) | Rt )

#define STRB32i(Rt, Rn, imm12)     ( (0b00<<30) | (0b11100100<<22) |  (imm12_scale((imm12),0) << 10) | (Rn << 5) | Rt )
#define STRH32i(Rt, Rn, imm12)     ( (0b01<<30) | (0b11100100<<22) |  (imm12_scale((imm12),1) << 10) | (Rn << 5) | Rt )
#define STR32i(Rt, Rn, imm12)      ( (0b10<<30) | (0b11100100<<22) |  (imm12_scale((imm12),2) << 10) | (Rn << 5) | Rt )

#define LDRSB32i(Rt, Rn, imm12) ( (0b00<<30) | (0b111001<<24) | (0b11<<22) | (imm12_scale(imm12,0)<<10) | (Rn<<5) | Rt )
#define LDRSH32i(Rt, Rn, imm12) ( (0b01<<30) | (0b111001<<24) | (0b11<<22) | (imm12_scale(imm12,1)<<10) | (Rn<<5) | Rt )

#define LDRB32(Rt, Rn, Rm)      ( (0b00<<30) | (0b111000011<<21) | (Rm<<16) | (0b010<<13) /*UXTW*/ | (0<<12) /*#0*/ | (0b10<<10) | (Rn << 5) | Rt )
#define LDRH32(Rt, Rn, Rm)      ( (0b01<<30) | (0b111000011<<21) | (Rm<<16) | (0b010<<13) /*UXTW*/ | (0<<12) /*#0*/ | (0b10<<10) | (Rn << 5) | Rt )
#define LDR32(Rt, Rn, Rm)       ( (0b10<<30) | (0b111000011<<21) | (Rm<<16) | (0b010<<13) /*UXTW*/ | (0<<12) /*#0*/ | (0b10<<10) | (Rn << 5) | Rt )

#define STRB32(Rt, Rn, Rm)         ( (0b00<<30) | (0b111000001<<21) | (Rm<<16) | (0b010<<13) /*UXTW*/ | (0<<12) /*#0*/ | (0b10<<10) | (Rn<<5) | Rt )
#define STRH32(Rt, Rn, Rm)         ( (0b01<<30) | (0b111000001<<21) | (Rm<<16) | (0b010<<13) /*UXTW*/ | (0<<12) /*#0*/ | (0b10<<10) | (Rn<<5) | Rt )
#define STR32(Rt, Rn, Rm)          ( (0b10<<30) | (0b111000001<<21) | (Rm<<16) | (0b010<<13) /*UXTW*/ | (0<<12) /*#0*/ | (0b10<<10) | (Rn<<5) | Rt )

#define LDR64i(Rt, Rn, imm12)      ( (0b11<<30) | (0b11100101<<22) |  (imm12_scale(imm12,3) << 10) | (Rn << 5) | Rt )

#define LDR64iwpost(Rt, Rn, simm9) ( (0b11<<30) | (0b111000010<<21) | ((simm9&511) << 12) | (0b01 << 10) | (Rn << 5) | Rt )
#define STR64iwpre(Rt, Rn, simm9)  ( (0b11<<30) | (0b111000000<<21) | ((simm9&511) << 12) | (0b11 << 10) | (Rn<<5) | Rt )

// branch to register
#define BR(Rn)                     ( (0b1101011<<25) | (0<<24) | (0<<23) | (0b00<<21) | (0b11111<<16) | (0b0000<<12) | (0<<11) /*A*/ | (0<<10) /*M*/ | (Rn<<5) | 0b00000 /*Rm*/ )

// branch with link to register
#define BLR(Rn)                    ( (0b1101011<<25) | (0<<24) | (0<<23) | (0b01<<21) | (0b11111<<16) | (0b0000<<12) | (0<<11) /*A*/ | (0<<10) /*M*/ | (Rn<<5) | 0b00000 /*Rm*/ )

#define FADD(Sd, Sn, Sm)         ( (0b000<<29) | (0b11110<<24) | (0b00<<22) | (1<<21) | (Sm<<16) | (0b001<<13) | (0<<12) /*op*/ | (0b10<<10) | (Sn<<5) | Sd )
#define FSUB(Sd, Sn, Sm)         ( (0b000<<29) | (0b11110<<24) | (0b00<<22) | (1<<21) | (Sm<<16) | (0b001<<13) | (1<<12) /*op*/ | (0b10<<10) | (Sn<<5) | Sd )
#define FMUL(Sd, Sn, Sm)         ( (0b000<<29) | (0b11110<<24) | (0b00<<22) | (1<<21) | (Sm<<16) | (0<<15) /*op*/ | (0b000<<12) | (0b10<<10) | (Sn<<5) | Sd )
#define FDIV(Sd, Sn, Sm)         ( (0b000<<29) | (0b11110<<24) | (0b00<<22) | (1<<21) | (Sm<<16) | (0b0001<<12) | (0b10<<10) | (Sn<<5) | Sd )

#define FCMP(Sn, Sm)             ( (0b000<<29) | (0b11110<<24) | (0b00<<22) | (1<<21) | (Sm<<16) | (0b00<<14) | (0b1000<<10) | (Sn<<5) | (0b00<<3) /*opc*/ | 0b000 )
#define FNEG(Sd, Sn)             ( (0b000<<29) | (0b11110<<24) | (0b00<<22) | (1<<21) | (0b0000<<17) | (0b10<<15) | (0b10000<<10) | (Sn<<5) | Sd )

// single precision to signed integer
#define FCVTZS(Rd, Sn)           ( (0<<31) | (0b00<<29) | (0b11110<<24) | (0b00<<22)  | (1<<21) | (0b11<<19) /*rmode*/ | (0b000<<16) /*opcode*/ | (0b000000<<10) | (Sn<<5) | Rd )
// signed integer to single precision
#define SCVTF(Sd, Rn)            ( (0<<31) | (0b00<<29) | (0b11110<<24) | (0b00<<22) | (1<<21) | (0b00<<19) /*rmode*/ | (0b010<<16) /*opcode*/ | (0b000000<<10) | (Rn<<5) | Sd )

#define VLDRi(St, Rn, imm12)     ( (0b10<<30) | (0b111<<27) | (1<<26) | (0b01<<24) | (0b01<<22) /*opc*/ | (imm12_scale(imm12,2) << 10) | (Rn<<5) | St )
#define VSTRi(St, Rn, imm12)     ( (0b10<<30) | (0b111<<27) | (1<<26) | (0b01<<24) | (0b00<<22) /*opc*/ | (imm12_scale(imm12,2) << 10) | (Rn<<5) | St )

/* ---- immediate helpers, taken from the Quake3e vm_aarch64.c ---- */

static uint32_t imm12_scale( const uint32_t imm12, const uint32_t scale )
{
	const uint32_t mask = (1<<scale) - 1;

	if ( imm12 & mask || imm12 >= 4096 * (1 << scale) )
		DIE( "can't encode offset %i with scale %i", imm12, (1 << scale) );

	return imm12 >> scale;
}


static int shifted_mask( const uint64_t v ) {
	const uint64_t m = v - 1;
	return ( ( ( m | v ) + 1 ) & m ) == 0;
}


static qboolean encode_logic_imm( const uint64_t v, uint32_t reg_size, uint32_t *res ) {
	uint64_t mask, imm;
	uint32_t size, len;
	uint32_t N, immr, imms;

	// determine element size
	if ( reg_size == 64 ) {
		mask = 0xFFFFFFFF;
		size = 32;
	} else {
		if ( v > 0xFFFFFFFF ) {
			return qfalse;
		}
		mask = 0xFFFF;
		size = 16;
	}
	for ( ;; ) {
		if ( ( v & mask ) != ( (v >> size) & mask ) || size == 1 ) {
			mask |= mask << size;
			size <<= 1;
			break;
		}
		size >>= 1;
		mask >>= size;
	}

	imm = v & mask;

	// early reject
	if ( !shifted_mask( imm ) && !shifted_mask( ~( imm | ~mask ) ) ) {
		return qfalse;
	}

	// rotate right to set leading zero and trailing one
	mask = 1ULL << ( size - 1 ) | 1;
	for ( immr = 0; immr < size; immr++ ) {
		if ( ( imm & mask ) == 1 ) {
			break;
		}
		imm = ( ( imm & 1 ) << ( size - 1 ) ) | ( imm >> 1 );
	}

	if ( immr == size ) {
		// all ones/zeros, unsupported
		return qfalse;
	}

	// count trailing bits set
	for ( len = 0; len < size; len++ ) {
		if ( ( ( imm >> len ) & 1 ) == 0 ) {
			break;
		}
	}

	N = ( size >> 6 ) & 1;
	imms = (63 & (64 - size*2)) | (len - 1);
	*res = ( N << 12 ) | ( (size - immr) << 6 ) | imms;

	return qtrue;
}


static void emit_mov32( vm_t *vm, int pass, unsigned reg, uint32_t imm32 )
{
	uint32_t immrs;

	if ( imm32 <= 0xFFFF ) {
		emit( MOVZ32( reg, imm32 ) );
		return;
	}

	if ( (imm32 & 0xFFFF) == 0 ) {
		emit( MOVZ32_16( reg, (imm32 >> 16) & 0xFFFF ) );
		return;
	}

	if ( ~imm32 <= 0xFFFF ) {
		emit( MOVN32( reg, ~imm32 ) );
		return;
	}

	if ( encode_logic_imm( imm32, 32, &immrs ) ) {
		emit( MOV32i( reg, immrs ) );
		return;
	}

	emit( MOVZ32( reg, imm32 & 0xFFFF ) );
	emit( MOVK32_16( reg, (imm32 >> 16) & 0xFFFF ) );
}


// Fixed-size variant: always emits 4 instructions regardless of value.
// Required so both compiler passes emit identical instruction counts
// even when an address is only known in the second pass.
static void emit_MOVXi64( vm_t *vm, int pass, uint32_t reg, uint64_t imm )
{
	emit( MOVZ64( reg, imm & 0xFFFF ) );
	emit( MOVK64_16( reg, (imm >> 16) & 0xFFFF ) );
	emit( MOVK64_32( reg, (imm >> 32) & 0xFFFF ) );
	emit( MOVK64_48( reg, (imm >> 48) & 0xFFFF ) );
}

/* check if imm fits an ADD/SUB immediate (possibly with LSL#12) */
static int enc_arith(unsigned imm, unsigned *out)
{
	if (imm <= 0xFFFu) {
		*out = imm;
		return 1;
	}
	if ((imm >> 12) <= 0xFFFu && (imm & 0xFFFu) == 0) {
		*out = (1u << 12) | (imm >> 12);
		return 1;
	}
	return 0;
}

/* load a vm_t pointer field into a 64-bit register (any struct offset) */
static void emit_ldrx(vm_t *vm, int pass, unsigned rd, unsigned rn, unsigned off)
{
	if (off < 32768u && (off & 7u) == 0) {
		emit(LDR64i(rd, rn, off));
	} else {
		emit_mov32(vm, pass, rTMP2, off);
		emit(ADD64(rTMP2, rn, rTMP2));
		emit(LDR64i(rd, rTMP2, 0));
	}
}

/* load a vm_t integer field into a 32-bit register (any struct offset) */
static void emit_ldrw(vm_t *vm, int pass, unsigned rd, unsigned rn, unsigned off)
{
	if (off < 16384u && (off & 3u) == 0) {
		emit(LDR32i(rd, rn, off));
	} else {
		emit_mov32(vm, pass, rTMP2, off);
		emit(ADD64(rTMP2, rn, rTMP2));
		emit(LDR32i(rd, rTMP2, 0));
	}
}

static unsigned get_comp(int op)
{
	switch (op) {
		case OP_EQ: return EQ;
		case OP_NE: return NE;
		case OP_LTI: return LT;
		case OP_LEI: return LE;
		case OP_GTI: return GT;
		case OP_GEI: return GE;
		case OP_LTU: return LO;
		case OP_LEU: return LS;
		case OP_GTU: return HI;
		case OP_GEU: return HS;
		case OP_EQF: return EQ;
		case OP_NEF: return NE;
		case OP_LTF: return MI;
		case OP_LEF: return LS;
		case OP_GTF: return GT;
		case OP_GEF: return GE;
		default: DIE("unexpected comparison op %d", op);
	}
	return 0;
}

/* check W0 < instructionCount, else ErrJump(W0). Clobbers W1, X12. */
#define CHECK_JUMP do { \
	unsigned __ok; \
	emit_mov32(vm, pass, rTMP1, (uint32_t)vm->instructionCount); \
	emit(CMP32(rTMP0, rTMP1)); \
	__ok = emit_bcond(vm, pass, LO); \
	emit_MOVXi64(vm, pass, rIP, (uintptr_t)ErrJump); \
	emit(BLR(rIP)); \
	fix_bcond(vm, pass, __ok, LO); \
} while (0)

/* integer compare-and-branch to VM target arg.i */
#define IJ(comparator) do { \
	emit_mov32(vm, pass, rTMP0, (uint32_t)arg.i); \
	CHECK_JUMP; \
	emit(LDR32i(rTMP0, rOPSTACK, 0)); \
	emit(SUB64i(rOPSTACK, rOPSTACK, 4)); \
	emit(LDR32i(rTMP1, rOPSTACK, 0)); \
	emit(SUB64i(rOPSTACK, rOPSTACK, 4)); \
	emit(CMP32(rTMP1, rTMP0)); \
	emit_far(vm, pass, comparator, vm->instructionPointers[arg.i]); \
} while (0)

/* float compare-and-branch to VM target arg.i */
#define FJ(comparator) do { \
	emit_mov32(vm, pass, rTMP0, (uint32_t)arg.i); \
	CHECK_JUMP; \
	emit(VLDRi(sTMP0, rOPSTACK, 0)); \
	emit(SUB64i(rOPSTACK, rOPSTACK, 4)); \
	emit(VLDRi(sTMP1, rOPSTACK, 0)); \
	emit(SUB64i(rOPSTACK, rOPSTACK, 4)); \
	emit(FCMP(sTMP1, sTMP0)); \
	emit_far(vm, pass, comparator, vm->instructionPointers[arg.i]); \
} while (0)

void VM_Compile(vm_t *vm, vmHeader_t *header)
{
	unsigned char *code;
	int i_count, pc = 0;
	int pass;

	vm->compiled = qfalse;

	vm->codeBase = NULL;
	vm->codeLength = 0;

	for (pass = 0; pass < 2; ++pass) {

	if (pass)
	{
		jit_protect(0);
		vm->codeBase = mmap(NULL, vm->codeLength, PROT_READ|PROT_WRITE, MMAP_FLAGS, -1, 0);
		if (vm->codeBase == MAP_FAILED)
			Com_Error(ERR_FATAL, "VM_CompileAArch64: can't mmap memory");
		vm->codeLength = 0;
	}

	/* prologue: save callee-saved regs, load VM state.
	 * C ABI on entry: x0 = vm, x1 = &programStack, x2 = opStack */
	emit(SUB64i(SP, SP, 64));
	emit(STP64(rOPSTACK, rOPSTACKBASE, SP, 0));
	emit(STP64(rCODEBASE, rPSTACK, SP, 16));
	emit(STP64(rDATABASE, rDATAMASK, SP, 32));
	emit(STP64(29, LR, SP, 48));

	emit_ldrx(vm, pass, rCODEBASE, 0, offsetof(vm_t, codeBase));
	emit_ldrx(vm, pass, rDATABASE, 0, offsetof(vm_t, dataBase));
	emit_ldrw(vm, pass, rDATAMASK, 0, offsetof(vm_t, dataMask));
	emit(LDR32i(rPSTACK, 1, 0)); /* w22 = *(&programStack) */
	emit(MOV64(rOPSTACK, 2));
	emit(MOV64(rOPSTACKBASE, rOPSTACK));

	{
		unsigned b = emit_bl(vm, pass); /* BL vmMain(): LR = epilogue */
		/* epilogue: return value = opstack top */
		emit(LDR32i(rTMP0, rOPSTACK, 0));
		emit(LDP64(rOPSTACK, rOPSTACKBASE, SP, 0));
		emit(LDP64(rCODEBASE, rPSTACK, SP, 16));
		emit(LDP64(rDATABASE, rDATAMASK, SP, 32));
		emit(LDP64(29, LR, SP, 48));
		emit(ADD64i(SP, SP, 64));
		emit(RET(LR));
		fix_bl(vm, pass, b);
	}

	code = (unsigned char *) header + header->codeOffset;
	pc = 0;

	for (i_count = 0; i_count < header->instructionCount; i_count++) {
		union {
			unsigned char b[4];
			unsigned int i;
		} arg;
		unsigned char op = code[pc++];

		vm->instructionPointers[i_count] = vm->codeLength;

		if (vm_opInfo[op] & opImm4)
		{
			memcpy(arg.b, &code[pc], 4);
			pc += 4;
		}
		else if (vm_opInfo[op] & opImm1)
		{
			arg.b[0] = code[pc];
			arg.b[1] = 0;
			arg.b[2] = 0;
			arg.b[3] = 0;
			++pc;
		}

		switch ( op )
		{
			case OP_UNDEF:
				break;

			case OP_IGNORE:
				NOTIMPL(op);
				break;

			case OP_BREAK:
				emit(BRK(0));
				break;

			case OP_ENTER:
			{
				unsigned imm;
				emit(STR64iwpre(LR, SP, -16)); /* push return address */
				if (enc_arith((unsigned)arg.i, &imm))
				{
					emit(SUB32i(rPSTACK, rPSTACK, imm)); /* pstack -= arg */
				}
				else
				{
					emit_mov32(vm, pass, rTMP0, (uint32_t)arg.i);
					emit(SUB32(rPSTACK, rPSTACK, rTMP0));
				}
				break;
			}

			case OP_LEAVE:
			{
				unsigned imm;
				if (enc_arith((unsigned)arg.i, &imm))
				{
					emit(ADD32i(rPSTACK, rPSTACK, imm)); /* pstack += arg */
				}
				else
				{
					emit_mov32(vm, pass, rTMP0, (uint32_t)arg.i);
					emit(ADD32(rPSTACK, rPSTACK, rTMP0));
				}
				emit(LDR64iwpost(LR, SP, 16)); /* pop return address */
				emit(RET(LR));
				break;
			}

			case OP_CALL:
			{
				unsigned b_lt, b_done;
				/* pop call target / syscall number */
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(CMP32i(rTMP0, 0)); /* syscall? */
				b_lt = emit_bcond(vm, pass, LT);
				/* local function call */
				CHECK_JUMP;
				emit_MOVXi64(vm, pass, rTMP1, (uintptr_t)vm->instructionPointers);
				emit(LSL32i(rTMP2, rTMP0, 2));       /* w2 = w0 * 4 */
				emit(ADD64(rTMP1, rTMP1, rTMP2));    /* x1 = table + w2 */
				emit(LDR32i(rTMP2, rTMP1, 0));       /* w2 = table[w0] */
				emit(ADD64(rTMP2, rCODEBASE, rTMP2));/* x2 = codeBase + w2 */
				emit(BLR(rTMP2));
				b_done = emit_b(vm, pass);
				/* syscall */
				fix_bcond(vm, pass, b_lt, LT);
				emit(MOV32(rTMP1, rPSTACK));         /* x1 = pstack */
				emit_MOVXi64(vm, pass, rIP, (uintptr_t)asmcall);
				emit(BLR(rIP));
				/* push return value */
				emit(ADD64i(rOPSTACK, rOPSTACK, 4));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				fix_b(vm, pass, b_done);
				break;
			}

			case OP_PUSH:
				emit(ADD64i(rOPSTACK, rOPSTACK, 4));
				break;

			case OP_POP:
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				break;

			case OP_CONST:
				emit(ADD64i(rOPSTACK, rOPSTACK, 4));
				emit_mov32(vm, pass, rTMP0, (uint32_t)arg.i);
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_LOCAL:
			{
				unsigned imm;
				emit(ADD64i(rOPSTACK, rOPSTACK, 4));
				if (enc_arith((unsigned)arg.i, &imm))
				{
					emit(ADD32i(rTMP0, rPSTACK, imm)); /* w0 = pstack + arg */
				}
				else
				{
					emit_mov32(vm, pass, rTMP0, (uint32_t)arg.i);
					emit(ADD32(rTMP0, rPSTACK, rTMP0));
				}
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;
			}

			case OP_JUMP:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				CHECK_JUMP;
				emit_MOVXi64(vm, pass, rTMP1, (uintptr_t)vm->instructionPointers);
				emit(LSL32i(rTMP2, rTMP0, 2));
				emit(ADD64(rTMP1, rTMP1, rTMP2));
				emit(LDR32i(rTMP2, rTMP1, 0));
				emit(ADD64(rTMP2, rCODEBASE, rTMP2));
				emit(BR(rTMP2));
				break;

			case OP_EQ:
				IJ(EQ);
				break;

			case OP_NE:
				IJ(NE);
				break;

			case OP_LTI:
				IJ(LT);
				break;

			case OP_LEI:
				IJ(LE);
				break;

			case OP_GTI:
				IJ(GT);
				break;

			case OP_GEI:
				IJ(GE);
				break;

			case OP_LTU:
				IJ(LO);
				break;

			case OP_LEU:
				IJ(LS);
				break;

			case OP_GTU:
				IJ(HI);
				break;

			case OP_GEU:
				IJ(HS);
				break;

			case OP_EQF:
				FJ(EQ);
				break;

			case OP_NEF:
				FJ(NE);
				break;

			case OP_LTF:
				FJ(MI);
				break;

			case OP_LEF:
				FJ(LS);
				break;

			case OP_GTF:
				FJ(GT);
				break;

			case OP_GEF:
				FJ(GE);
				break;

			case OP_LOAD1:
				emit(LDR32i(rTMP0, rOPSTACK, 0)); /* w0 = *opstack */
				emit(AND32(rTMP0, rTMP0, rDATAMASK));
				emit(LDRB32(rTMP0, rDATABASE, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_LOAD2:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(AND32(rTMP0, rTMP0, rDATAMASK));
				emit(LDRH32(rTMP0, rDATABASE, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_LOAD4:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(AND32(rTMP0, rTMP0, rDATAMASK));
				emit(LDR32(rTMP0, rDATABASE, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_STORE1:
				emit(LDR32i(rTMP0, rOPSTACK, 0)); /* value */
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0)); /* address */
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(AND32(rTMP1, rTMP1, rDATAMASK));
				emit(STRB32(rTMP0, rDATABASE, rTMP1));
				break;

			case OP_STORE2:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(AND32(rTMP1, rTMP1, rDATAMASK));
				emit(STRH32(rTMP0, rDATABASE, rTMP1));
				break;

			case OP_STORE4:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(AND32(rTMP1, rTMP1, rDATAMASK));
				emit(STR32(rTMP0, rDATABASE, rTMP1));
				break;

			case OP_ARG:
				emit(LDR32i(rTMP0, rOPSTACK, 0)); /* value */
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(ADD32i(rTMP1, rPSTACK, arg.b[0])); /* w1 = pstack + arg */
				emit(AND32(rTMP1, rTMP1, rDATAMASK));
				emit(STR32(rTMP0, rDATABASE, rTMP1));
				break;

			case OP_BLOCK_COPY:
				emit(LDR32i(rTMP1, rOPSTACK, 0)); /* src */
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP0, rOPSTACK, 0)); /* dst */
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit_mov32(vm, pass, rTMP2, (uint32_t)arg.i);
				emit_MOVXi64(vm, pass, rTMP3, (uintptr_t)VM_BlockCopy);
				emit(BLR(rTMP3));
				break;

			case OP_SEX8:
				emit(LDRSB32i(rTMP0, rOPSTACK, 0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_SEX16:
				emit(LDRSH32i(rTMP0, rOPSTACK, 0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_NEGI:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(NEG32(rTMP0, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_ADD:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(ADD32(rTMP0, rTMP1, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_SUB:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(SUB32(rTMP0, rTMP1, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_DIVI:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(SDIV32(rTMP0, rTMP1, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_DIVU:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(UDIV32(rTMP0, rTMP1, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_MODI:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(SDIV32(rTMP2, rTMP1, rTMP0));
				emit(MSUB32(rTMP0, rTMP0, rTMP2, rTMP1));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_MODU:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(UDIV32(rTMP2, rTMP1, rTMP0));
				emit(MSUB32(rTMP0, rTMP0, rTMP2, rTMP1));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_MULI:
			case OP_MULU:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(MUL32(rTMP0, rTMP1, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_BAND:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(AND32(rTMP0, rTMP1, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_BOR:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(ORR32(rTMP0, rTMP1, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_BXOR:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(EOR32(rTMP0, rTMP1, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_BCOM:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(MVN32(rTMP0, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_LSH:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(LSL32(rTMP0, rTMP1, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_RSHI:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(ASR32(rTMP0, rTMP1, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_RSHU:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(LDR32i(rTMP1, rOPSTACK, 0));
				emit(LSR32(rTMP0, rTMP1, rTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;

			case OP_NEGF:
				emit(VLDRi(sTMP0, rOPSTACK, 0));
				emit(FNEG(sTMP0, sTMP0));
				emit(VSTRi(sTMP0, rOPSTACK, 0));
				break;

			case OP_ADDF:
				emit(VLDRi(sTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(VLDRi(sTMP1, rOPSTACK, 0));
				emit(FADD(sTMP0, sTMP1, sTMP0));
				emit(VSTRi(sTMP0, rOPSTACK, 0));
				break;

			case OP_SUBF:
				emit(VLDRi(sTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(VLDRi(sTMP1, rOPSTACK, 0));
				emit(FSUB(sTMP0, sTMP1, sTMP0));
				emit(VSTRi(sTMP0, rOPSTACK, 0));
				break;

			case OP_DIVF:
				emit(VLDRi(sTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(VLDRi(sTMP1, rOPSTACK, 0));
				emit(FDIV(sTMP0, sTMP1, sTMP0));
				emit(VSTRi(sTMP0, rOPSTACK, 0));
				break;

			case OP_MULF:
				emit(VLDRi(sTMP0, rOPSTACK, 0));
				emit(SUB64i(rOPSTACK, rOPSTACK, 4));
				emit(VLDRi(sTMP1, rOPSTACK, 0));
				emit(FMUL(sTMP0, sTMP1, sTMP0));
				emit(VSTRi(sTMP0, rOPSTACK, 0));
				break;

			case OP_CVIF:
				emit(LDR32i(rTMP0, rOPSTACK, 0));
				emit(SCVTF(sTMP0, rTMP0));
				emit(VSTRi(sTMP0, rOPSTACK, 0));
				break;

			case OP_CVFI:
				emit(VLDRi(sTMP0, rOPSTACK, 0));
				emit(FCVTZS(rTMP0, sTMP0));
				emit(STR32i(rTMP0, rOPSTACK, 0));
				break;
		}
	}

	// never reached
	emit(BRK(0));

	} // pass

	jit_protect(1);

	if (mprotect(vm->codeBase, vm->codeLength, PROT_READ|PROT_EXEC)) {
		VM_Destroy_Compiled(vm);
		DIE("mprotect failed");
	}

	// clear icache, http://blogs.arm.com/software-enablement/141-caches-and-self-modifying-code/
	__clear_cache(vm->codeBase, (byte *)vm->codeBase + vm->codeLength);

	vm->destroy = VM_Destroy_Compiled;
	vm->compiled = qtrue;
}

int VM_CallCompiled(vm_t *vm, int *args)
{
	byte	stack[OPSTACK_SIZE + 15];
	int	*opStack;
	int	programStack = vm->programStack;
	int	stackOnEntry = programStack;
	byte	*image = vm->dataBase;
	int	*argPointer;
	int	retVal;

	currentVM = vm;

	vm->currentlyInterpreting = qtrue;

	programStack -= ( 8 + 4 * MAX_VMMAIN_ARGS );
	argPointer = (int *)&image[ programStack + 8 ];
	memcpy( argPointer, args, 4 * MAX_VMMAIN_ARGS );
	argPointer[-1] = 0;
	argPointer[-2] = -1;


	opStack = PADP(stack, 16);
	*opStack = 0xDEADBEEF;

	/* call generated code */
	{
		int (*entry)(vm_t*, int*, int*);

		entry = (void *)(vm->codeBase);
		retVal = entry(vm, &programStack, opStack);
	}

	if(*opStack != 0xDEADBEEF)
	{
		Com_Error(ERR_DROP, "opStack corrupted in compiled code");
	}

	if(programStack != stackOnEntry - (8 + 4 * MAX_VMMAIN_ARGS))
		Com_Error(ERR_DROP, "programStack corrupted in compiled code");

	vm->programStack = stackOnEntry;
	vm->currentlyInterpreting = qfalse;

	return retVal;
}

#endif // __aarch64__
