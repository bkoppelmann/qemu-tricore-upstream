/*
 * RISC-V emulation for qemu: main translation routines.
 *
 * Author: Sagar Karandikar, sagark@eecs.berkeley.edu
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "disas/disas.h"
#include "tcg-op.h"
#include "exec/cpu_ldst.h"

#include "exec/helper-proto.h"
#include "exec/helper-gen.h"

#include "instmap.h"

/* global register indices */
static TCGv_ptr cpu_env;
static TCGv cpu_gpr[32], cpu_PC, cpu_fpr[32];
static TCGv load_reservation;

#include "exec/gen-icount.h"

typedef struct DisasContext {
    struct TranslationBlock *tb;
    target_ulong pc;
    uint32_t opcode;
    int singlestep_enabled;
    int mem_idx;
    int bstate;
} DisasContext;

enum {
    BS_NONE     = 0, /* When seen outside of translation while loop, indicates*/
                     /* need to exit tb due to end of page. */
    BS_STOP     = 1, /* Need to exit tb for syscall, sret, etc. */
    BS_BRANCH   = 2, /* Need to exit tb for branch, jal, etc. */
};

static const char * const regnames[] = {
  "zero", "ra  ", "sp  ", "gp  ", "tp  ", "t0  ",  "t1  ",  "t2  ",
  "s0  ", "s1  ", "a0  ", "a1  ", "a2  ", "a3  ",  "a4  ",  "a5  ",
  "a6  ", "a7  ", "s2  ", "s3  ", "s4  ", "s5  ",  "s6  ",  "s7  ",
  "s8  ", "s9  ", "s10 ", "s11 ", "t3  ", "t4  ",  "t5  ",  "t6  "
};

static const char * const fpr_regnames[] = {
  "ft0", "ft1", "ft2",  "ft3",  "ft4", "ft5", "ft6",  "ft7",
  "fs0", "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",
  "fa6", "fa7", "fs2",  "fs3",  "fs4", "fs5", "fs6",  "fs7",
  "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"
};

#define RISCV_DEBUG(fmt, ...)                                                  \
    do {                                                                      \
        if (RISCV_DEBUG_DISAS) {                                               \
            qemu_log_mask(CPU_LOG_TB_IN_ASM,                                  \
                          TARGET_FMT_lx ": %08x " fmt "\n",                   \
                          ctx->pc, ctx->opcode , ## __VA_ARGS__);             \
        }                                                                     \
    } while (0)

#define LOG_DISAS(...)                                                        \
    do {                                                                      \
        if (RISCV_DEBUG_DISAS) {                                               \
            qemu_log_mask(CPU_LOG_TB_IN_ASM, ## __VA_ARGS__);                 \
        }                                                                     \
    } while (0)

static void kill_unknown(DisasContext *ctx, int excp)
{
    /* generate_exception(ctx, excp); */
    ctx->bstate = BS_STOP;
}
/* Wrapper for getting reg values - need to check of reg is zero since
 * cpu_gpr[0] is not actually allocated
 */
static inline void gen_get_gpr(TCGv t, int reg_num)
{
    if (reg_num == 0) {
        tcg_gen_movi_tl(t, 0);
    } else {
        tcg_gen_mov_tl(t, cpu_gpr[reg_num]);
    }
}

/* Wrapper for setting reg values - need to check of reg is zero since
 * cpu_gpr[0] is not actually allocated. this is more for safety purposes,
 * since we usually avoid calling the OP_TYPE_gen function if we see a write to
 * $zero
 */
static inline void gen_set_gpr(int reg_num_dst, TCGv t)
{
    if (reg_num_dst != 0) {
        tcg_gen_mov_tl(cpu_gpr[reg_num_dst], t);
    }
}

static void gen_arith(DisasContext *ctx, uint32_t opc,
                      int rd, int rs1, int rs2)
{
    TCGv source1, source2;

    source1 = tcg_temp_new();
    source2 = tcg_temp_new();

    gen_get_gpr(source1, rs1);
    gen_get_gpr(source2, rs2);

    switch (opc) {

    case OPC_RISC_ADD:
        tcg_gen_add_tl(source1, source1, source2);
        break;
    case OPC_RISC_SUB:
        tcg_gen_sub_tl(source1, source1, source2);
        break;
    case OPC_RISC_SLL:
        tcg_gen_andi_tl(source2, source2, 0x3F);
        tcg_gen_shl_tl(source1, source1, source2);
        break;
    case OPC_RISC_SLT:
        tcg_gen_setcond_tl(TCG_COND_LT, source1, source1, source2);
        break;
    case OPC_RISC_SLTU:
        tcg_gen_setcond_tl(TCG_COND_LTU, source1, source1, source2);
        break;
    case OPC_RISC_XOR:
        tcg_gen_xor_tl(source1, source1, source2);
        break;
    case OPC_RISC_SRL:
        tcg_gen_andi_tl(source2, source2, 0x3F);
        tcg_gen_shr_tl(source1, source1, source2);
        break;
    case OPC_RISC_SRA:
        tcg_gen_andi_tl(source2, source2, 0x3F);
        tcg_gen_sar_tl(source1, source1, source2);
        break;
    case OPC_RISC_OR:
        tcg_gen_or_tl(source1, source1, source2);
        break;
    case OPC_RISC_AND:
        tcg_gen_and_tl(source1, source1, source2);
        break;
    case OPC_RISC_MUL:
        tcg_gen_muls2_tl(source1, source2, source1, source2);
        break;
    case OPC_RISC_MULH:
        tcg_gen_muls2_tl(source2, source1, source1, source2);
        break;
    case OPC_RISC_MULHSU:
        gen_helper_mulhsu(source1, cpu_env, source1, source2);
        break;
    case OPC_RISC_MULHU:
        tcg_gen_mulu2_tl(source2, source1, source1, source2);
        break;
    case OPC_RISC_DIV:
        {
            TCGv spec_source1, spec_source2;
            TCGv cond1, cond2;
            TCGLabel *handle_zero = gen_new_label();
            TCGLabel *handle_overflow = gen_new_label();
            TCGLabel *done = gen_new_label();
            spec_source1 = tcg_temp_local_new();
            spec_source2 = tcg_temp_local_new();
            cond1 = tcg_temp_local_new();
            cond2 = tcg_temp_local_new();

            gen_get_gpr(spec_source1, rs1);
            gen_get_gpr(spec_source2, rs2);
            tcg_gen_brcondi_tl(TCG_COND_EQ, spec_source2, 0x0, handle_zero);

            /* now, use temp reg to check if both overflow conditions
               satisfied */
            tcg_gen_setcondi_tl(TCG_COND_EQ, cond2, spec_source2,
                                0xFFFFFFFFFFFFFFFF); /* divisor = -1 */
            tcg_gen_setcondi_tl(TCG_COND_EQ, cond1, spec_source1,
                                0x8000000000000000);
            tcg_gen_and_tl(cond1, cond1, cond2);

            tcg_gen_brcondi_tl(TCG_COND_EQ, cond1, 1, handle_overflow);
            /* normal case */
            tcg_gen_div_tl(spec_source1, spec_source1, spec_source2);
            tcg_gen_br(done);
            /* special zero case */
            gen_set_label(handle_zero);
            tcg_gen_movi_tl(spec_source1, -1);
            tcg_gen_br(done);
            /* special overflow case */
            gen_set_label(handle_overflow);
            tcg_gen_movi_tl(spec_source1, 0x8000000000000000);
            /* done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, spec_source1);
            tcg_temp_free(spec_source1);
            tcg_temp_free(spec_source2);
            tcg_temp_free(cond1);
            tcg_temp_free(cond2);
        }
        break;
    case OPC_RISC_DIVU:
        {
            TCGv spec_source1, spec_source2;
            TCGLabel *handle_zero = gen_new_label();
            TCGLabel *done = gen_new_label();
            spec_source1 = tcg_temp_local_new();
            spec_source2 = tcg_temp_local_new();

            gen_get_gpr(spec_source1, rs1);
            gen_get_gpr(spec_source2, rs2);
            tcg_gen_brcondi_tl(TCG_COND_EQ, spec_source2, 0x0, handle_zero);

            /* normal case */
            tcg_gen_divu_tl(spec_source1, spec_source1, spec_source2);
            tcg_gen_br(done);
            /* special zero case */
            gen_set_label(handle_zero);
            tcg_gen_movi_tl(spec_source1, -1);
            tcg_gen_br(done);
            /* done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, spec_source1);
            tcg_temp_free(spec_source1);
            tcg_temp_free(spec_source2);
        }
        break;
    case OPC_RISC_REM:
        {
            TCGv spec_source1, spec_source2;
            TCGv cond1, cond2;
            TCGLabel *handle_zero = gen_new_label();
            TCGLabel *handle_overflow = gen_new_label();
            TCGLabel *done = gen_new_label();
            spec_source1 = tcg_temp_local_new();
            spec_source2 = tcg_temp_local_new();
            cond1 = tcg_temp_local_new();
            cond2 = tcg_temp_local_new();

            gen_get_gpr(spec_source1, rs1);
            gen_get_gpr(spec_source2, rs2);
            tcg_gen_brcondi_tl(TCG_COND_EQ, spec_source2, 0x0, handle_zero);

            /* now, use temp reg to check if both overflow conditions
               satisfied */
            tcg_gen_setcondi_tl(TCG_COND_EQ, cond2, spec_source2,
                                0xFFFFFFFFFFFFFFFF); /* divisor = -1 */
            tcg_gen_setcondi_tl(TCG_COND_EQ, cond1, spec_source1,
                                0x8000000000000000);
            tcg_gen_and_tl(cond1, cond1, cond2);

            tcg_gen_brcondi_tl(TCG_COND_EQ, cond1, 1, handle_overflow);
            /* normal case */
            tcg_gen_rem_tl(spec_source1, spec_source1, spec_source2);
            tcg_gen_br(done);
            /* special zero case */
            gen_set_label(handle_zero);
            tcg_gen_mov_tl(spec_source1, spec_source1); /* even though it's a
                                                        nop, just for clarity */
            tcg_gen_br(done);
            /* special overflow case */
            gen_set_label(handle_overflow);
            tcg_gen_movi_tl(spec_source1, 0);
            /* done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, spec_source1);
            tcg_temp_free(spec_source1);
            tcg_temp_free(spec_source2);
            tcg_temp_free(cond1);
            tcg_temp_free(cond2);
        }
        break;
    case OPC_RISC_REMU:
        {
            TCGv spec_source1, spec_source2;
            TCGLabel *handle_zero = gen_new_label();
            TCGLabel *done = gen_new_label();
            spec_source1 = tcg_temp_local_new();
            spec_source2 = tcg_temp_local_new();

            gen_get_gpr(spec_source1, rs1);
            gen_get_gpr(spec_source2, rs2);
            tcg_gen_brcondi_tl(TCG_COND_EQ, spec_source2, 0x0, handle_zero);

            /* normal case */
            tcg_gen_remu_tl(spec_source1, spec_source1, spec_source2);
            tcg_gen_br(done);
            /* special zero case */
            gen_set_label(handle_zero);
            tcg_gen_mov_tl(spec_source1, spec_source1); /* even though it's a
                                                        nop, just for clarity */
            tcg_gen_br(done);
            /* done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, spec_source1);
            tcg_temp_free(spec_source1);
            tcg_temp_free(spec_source2);
        }
        break;
    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;

    }

    /* set and free */
    gen_set_gpr(rd, source1);
    tcg_temp_free(source1);
    tcg_temp_free(source2);
}

/* lower 12 bits of imm are valid */
static void gen_arith_imm(DisasContext *ctx, uint32_t opc,
                      int rd, int rs1, int16_t imm)
{
    TCGv source1;
    source1 = tcg_temp_new();
    gen_get_gpr(source1, rs1);
    target_long uimm = (target_long)imm; /* sign ext 16->64 bits */

    switch (opc) {
    case OPC_RISC_ADDI:
        tcg_gen_addi_tl(source1, source1, uimm);
        break;
    case OPC_RISC_SLTI:
        tcg_gen_setcondi_tl(TCG_COND_LT, source1, source1, uimm);
        break;
    case OPC_RISC_SLTIU:
        tcg_gen_setcondi_tl(TCG_COND_LTU, source1, source1, uimm);
        break;
    case OPC_RISC_XORI:
        tcg_gen_xori_tl(source1, source1, uimm);
        break;
    case OPC_RISC_ORI:
        tcg_gen_ori_tl(source1, source1, uimm);
        break;
    case OPC_RISC_ANDI:
        tcg_gen_andi_tl(source1, source1, uimm);
        break;
    case OPC_RISC_SLLI: /* TODO: add immediate upper bits check? */
        tcg_gen_shli_tl(source1, source1, uimm);
        break;
    case OPC_RISC_SHIFT_RIGHT_I: /* SRLI, SRAI, TODO: upper bits check */
        /* differentiate on IMM */
        if (uimm & 0x400) {
            /* SRAI */
            tcg_gen_sari_tl(source1, source1, uimm ^ 0x400);
        } else {
            tcg_gen_shri_tl(source1, source1, uimm);
        }
        break;
    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;
    }

    gen_set_gpr(rd, source1);
    tcg_temp_free(source1);
}

/* lower 12 bits of imm are valid */
static void gen_arith_imm_w(DisasContext *ctx, uint32_t opc,
                      int rd, int rs1, int16_t imm)
{
    TCGv source1;
    source1 = tcg_temp_new();
    gen_get_gpr(source1, rs1);
    target_long uimm = (target_long)imm; /* sign ext 16->64 bits */

    switch (opc) {
    case OPC_RISC_ADDIW:
        tcg_gen_addi_tl(source1, source1, uimm);
        tcg_gen_ext32s_tl(source1, source1);
        break;
    case OPC_RISC_SLLIW: /* TODO: add immediate upper bits check? */
        tcg_gen_shli_tl(source1, source1, uimm);
        tcg_gen_ext32s_tl(source1, source1);
        break;
    case OPC_RISC_SHIFT_RIGHT_IW: /* SRLIW, SRAIW, TODO: upper bits check */
        /* differentiate on IMM */
        if (uimm & 0x400) {
            /* SRAI */
            /* first, trick to get it to act like working on 32 bits: */
            tcg_gen_shli_tl(source1, source1, 32);
            /* now shift back to the right by shamt + 32 to get proper upper */
            /* bits filling */
            tcg_gen_sari_tl(source1, source1, (uimm ^ 0x400) + 32);
            tcg_gen_ext32s_tl(source1, source1);
        } else {
            /* first, trick to get it to act like working on 32 bits (get rid */
            /* of upper 32): */
            tcg_gen_shli_tl(source1, source1, 32);
            /* now shift back to the right by shamt + 32 to get proper upper */
            /* bits filling */
            tcg_gen_shri_tl(source1, source1, uimm + 32);
            tcg_gen_ext32s_tl(source1, source1);
        }
        break;
    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;
    }
    gen_set_gpr(rd, source1);
    tcg_temp_free(source1);
}

static void gen_arith_w(DisasContext *ctx, uint32_t opc,
                        int rd, int rs1, int rs2)
{
    TCGv source1, source2;
    source1 = tcg_temp_new();
    source2 = tcg_temp_new();
    gen_get_gpr(source1, rs1);
    gen_get_gpr(source2, rs2);

    switch (opc) {
    case OPC_RISC_ADDW:
        tcg_gen_add_tl(source1, source1, source2);
        tcg_gen_ext32s_tl(source1, source1);
        break;
    case OPC_RISC_SUBW:
        tcg_gen_sub_tl(source1, source1, source2);
        tcg_gen_ext32s_tl(source1, source1);
        break;
    case OPC_RISC_SLLW:
        tcg_gen_andi_tl(source2, source2, 0x1F);
        tcg_gen_shl_tl(source1, source1, source2);
        tcg_gen_ext32s_tl(source1, source1);
        break;
    case OPC_RISC_SRLW:
        tcg_gen_andi_tl(source1, source1,
                        0x00000000FFFFFFFFLL); /* clear upper 32 */
        tcg_gen_andi_tl(source2, source2, 0x1F);
        tcg_gen_shr_tl(source1, source1, source2); /* do actual right shift */
        tcg_gen_ext32s_tl(source1, source1); /* sign ext */
        break;
    case OPC_RISC_SRAW:
        /* first, trick to get it to act like working on 32 bits (get rid of */
        /* upper 32) */
        tcg_gen_shli_tl(source1, source1, 32); /* clear upper 32 */
        tcg_gen_sari_tl(source1, source1,
                        32); /* smear the sign bit into upper 32 */
        tcg_gen_andi_tl(source2, source2, 0x1F);
        tcg_gen_sar_tl(source1, source1,
                       source2); /* do the actual right shift */
        tcg_gen_ext32s_tl(source1, source1); /* sign ext */
        break;
    case OPC_RISC_MULW:
        tcg_gen_muls2_tl(source1, source2, source1, source2);
        tcg_gen_ext32s_tl(source1, source1);
        break;
    case OPC_RISC_DIVW:
        {
            TCGv spec_source1, spec_source2;
            TCGv cond1, cond2;
            TCGLabel *handle_zero = gen_new_label();
            TCGLabel *handle_overflow = gen_new_label();
            TCGLabel *done = gen_new_label();
            spec_source1 = tcg_temp_local_new();
            spec_source2 = tcg_temp_local_new();
            cond1 = tcg_temp_local_new();
            cond2 = tcg_temp_local_new();

            gen_get_gpr(spec_source1, rs1);
            gen_get_gpr(spec_source2, rs2);
            tcg_gen_ext32s_tl(spec_source1, spec_source1);
            tcg_gen_ext32s_tl(spec_source2, spec_source2);

            tcg_gen_brcondi_tl(TCG_COND_EQ, spec_source2, 0x0, handle_zero);

            /* now, use temp reg to check if both overflow conditions
               satisfied */
            tcg_gen_setcondi_tl(TCG_COND_EQ, cond2, spec_source2,
                                0xFFFFFFFFFFFFFFFF); /* divisor = -1 */
            tcg_gen_setcondi_tl(TCG_COND_EQ, cond1, spec_source1,
                                0x8000000000000000);
            tcg_gen_and_tl(cond1, cond1, cond2);

            tcg_gen_brcondi_tl(TCG_COND_EQ, cond1, 1, handle_overflow);
            /* normal case */
            tcg_gen_div_tl(spec_source1, spec_source1, spec_source2);
            tcg_gen_br(done);
            /* special zero case */
            gen_set_label(handle_zero);
            tcg_gen_movi_tl(spec_source1, -1);
            tcg_gen_br(done);
            /* special overflow case */
            gen_set_label(handle_overflow);
            tcg_gen_movi_tl(spec_source1, 0x8000000000000000);
            /* done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, spec_source1);
            tcg_temp_free(spec_source1);
            tcg_temp_free(spec_source2);
            tcg_temp_free(cond1);
            tcg_temp_free(cond2);
            tcg_gen_ext32s_tl(source1, source1);
        }
        break;
    case OPC_RISC_DIVUW:
        {
            TCGv spec_source1, spec_source2;
            TCGLabel *handle_zero = gen_new_label();
            TCGLabel *done = gen_new_label();
            spec_source1 = tcg_temp_local_new();
            spec_source2 = tcg_temp_local_new();

            gen_get_gpr(spec_source1, rs1);
            gen_get_gpr(spec_source2, rs2);
            tcg_gen_ext32u_tl(spec_source1, spec_source1);
            tcg_gen_ext32u_tl(spec_source2, spec_source2);

            tcg_gen_brcondi_tl(TCG_COND_EQ, spec_source2, 0x0, handle_zero);

            /* normal case */
            tcg_gen_divu_tl(spec_source1, spec_source1, spec_source2);
            tcg_gen_br(done);
            /* special zero case */
            gen_set_label(handle_zero);
            tcg_gen_movi_tl(spec_source1, -1);
            tcg_gen_br(done);
            /* done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, spec_source1);
            tcg_temp_free(spec_source1);
            tcg_temp_free(spec_source2);
            tcg_gen_ext32s_tl(source1, source1);
        }
        break;
    case OPC_RISC_REMW:
        {
            TCGv spec_source1, spec_source2;
            TCGv cond1, cond2;
            TCGLabel *handle_zero = gen_new_label();
            TCGLabel *handle_overflow = gen_new_label();
            TCGLabel *done = gen_new_label();
            spec_source1 = tcg_temp_local_new();
            spec_source2 = tcg_temp_local_new();
            cond1 = tcg_temp_local_new();
            cond2 = tcg_temp_local_new();

            gen_get_gpr(spec_source1, rs1);
            gen_get_gpr(spec_source2, rs2);
            tcg_gen_ext32s_tl(spec_source1, spec_source1);
            tcg_gen_ext32s_tl(spec_source2, spec_source2);

            tcg_gen_brcondi_tl(TCG_COND_EQ, spec_source2, 0x0, handle_zero);

            /* now, use temp reg to check if both overflow conditions
               satisfied */
            tcg_gen_setcondi_tl(TCG_COND_EQ, cond2, spec_source2,
                                0xFFFFFFFFFFFFFFFF); /* divisor = -1 */
            tcg_gen_setcondi_tl(TCG_COND_EQ, cond1, spec_source1,
                                0x8000000000000000);
            tcg_gen_and_tl(cond1, cond1, cond2);

            tcg_gen_brcondi_tl(TCG_COND_EQ, cond1, 1, handle_overflow);
            /* normal case */
            tcg_gen_rem_tl(spec_source1, spec_source1, spec_source2);
            tcg_gen_br(done);
            /* special zero case */
            gen_set_label(handle_zero);
            tcg_gen_mov_tl(spec_source1, spec_source1); /* even though it's a
                                                        nop, just for clarity */
            tcg_gen_br(done);
            /* special overflow case */
            gen_set_label(handle_overflow);
            tcg_gen_movi_tl(spec_source1, 0);
            /* done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, spec_source1);
            tcg_temp_free(spec_source1);
            tcg_temp_free(spec_source2);
            tcg_temp_free(cond1);
            tcg_temp_free(cond2);
            tcg_gen_ext32s_tl(source1, source1);
        }
        break;
    case OPC_RISC_REMUW:
        {
            TCGv spec_source1, spec_source2;
            TCGLabel *handle_zero = gen_new_label();
            TCGLabel *done = gen_new_label();
            spec_source1 = tcg_temp_local_new();
            spec_source2 = tcg_temp_local_new();

            gen_get_gpr(spec_source1, rs1);
            gen_get_gpr(spec_source2, rs2);
            tcg_gen_ext32u_tl(spec_source1, spec_source1);
            tcg_gen_ext32u_tl(spec_source2, spec_source2);

            tcg_gen_brcondi_tl(TCG_COND_EQ, spec_source2, 0x0, handle_zero);
            /* normal case */
            tcg_gen_remu_tl(spec_source1, spec_source1, spec_source2);
            tcg_gen_br(done);
            /* special zero case */
            gen_set_label(handle_zero);
            tcg_gen_mov_tl(spec_source1, spec_source1); /* even though it's a
                                                        nop, just for clarity */
            tcg_gen_br(done);
            /* done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, spec_source1);
            tcg_temp_free(spec_source1);
            tcg_temp_free(spec_source2);
            tcg_gen_ext32s_tl(source1, source1);
        }
        break;
    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;
    }
    gen_set_gpr(rd, source1);
    tcg_temp_free(source1);
    tcg_temp_free(source2);
}

static inline bool use_goto_tb(DisasContext *ctx, target_ulong dest)
{
    if (unlikely(ctx->singlestep_enabled)) {
        return false;
    }

#ifndef CONFIG_USER_ONLY
    return (ctx->tb->pc & TARGET_PAGE_MASK) == (dest & TARGET_PAGE_MASK);
#else
    return true;
#endif
}

static inline void gen_goto_tb(DisasContext *ctx, int n, target_ulong dest)
{
    if (use_goto_tb(ctx, dest)) {
        /* we only allow direct chaining when the jump is to the same page
           otherwise, we could produce incorrect chains when address spaces
           change. see
           http://lists.gnu.org/archive/html/qemu-devel/2007-06/msg00213.html */
        tcg_gen_goto_tb(n);
        tcg_gen_movi_tl(cpu_PC, dest);
        tcg_gen_exit_tb((uintptr_t)ctx->tb + n);
    } else {
        tcg_gen_movi_tl(cpu_PC, dest);
        if (ctx->singlestep_enabled) {
            /* raise excp debug */
        }
        tcg_gen_exit_tb(0);
    }
}

static void gen_branch(DisasContext *ctx, uint32_t opc,
                       int rs1, int rs2, int16_t bimm) {

    /* TODO: misaligned insn (see jalr) */
    TCGLabel *l = gen_new_label();
    TCGv source1, source2;
    source1 = tcg_temp_new();
    source2 = tcg_temp_new();
    gen_get_gpr(source1, rs1);
    gen_get_gpr(source2, rs2);
    target_ulong ubimm = (target_long)bimm; /* sign ext 16->64 bits */

    switch (opc) {
    case OPC_RISC_BEQ:
        tcg_gen_brcond_tl(TCG_COND_EQ, source1, source2, l);
        break;
    case OPC_RISC_BNE:
        tcg_gen_brcond_tl(TCG_COND_NE, source1, source2, l);
        break;
    case OPC_RISC_BLT:
        tcg_gen_brcond_tl(TCG_COND_LT, source1, source2, l);
        break;
    case OPC_RISC_BGE:
        tcg_gen_brcond_tl(TCG_COND_GE, source1, source2, l);
        break;
    case OPC_RISC_BLTU:
        tcg_gen_brcond_tl(TCG_COND_LTU, source1, source2, l);
        break;
    case OPC_RISC_BGEU:
        tcg_gen_brcond_tl(TCG_COND_GEU, source1, source2, l);
        break;
    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;
    }
    gen_goto_tb(ctx, 1, ctx->pc + 4); /* must use this for safety */

    gen_set_label(l); /* branch taken */
    gen_goto_tb(ctx, 0, ctx->pc + ubimm); /* must use this for safety */

    tcg_temp_free(source1);
    tcg_temp_free(source2);
    ctx->bstate = BS_BRANCH;
}

static void gen_load(DisasContext *ctx, uint32_t opc,
                     int rd, int rs1, int16_t imm)
{

    target_long uimm = (target_long)imm; /* sign ext 16->64 bits */

    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();

    gen_get_gpr(t0, rs1);
    tcg_gen_addi_tl(t0, t0, uimm);

    switch (opc) {

    case OPC_RISC_LB:
        tcg_gen_qemu_ld_tl(t1, t0, ctx->mem_idx, MO_SB);
        break;
    case OPC_RISC_LH:
        tcg_gen_qemu_ld_tl(t1, t0, ctx->mem_idx, MO_LESW);
        break;
    case OPC_RISC_LW:
        tcg_gen_qemu_ld_tl(t1, t0, ctx->mem_idx, MO_LESL);
        break;
    case OPC_RISC_LD:
        tcg_gen_qemu_ld_tl(t1, t0, ctx->mem_idx, MO_LEQ);
        break;
    case OPC_RISC_LBU:
        tcg_gen_qemu_ld_tl(t1, t0, ctx->mem_idx, MO_UB);
        break;
    case OPC_RISC_LHU:
        tcg_gen_qemu_ld_tl(t1, t0, ctx->mem_idx, MO_LEUW);
        break;
    case OPC_RISC_LWU:
        tcg_gen_qemu_ld_tl(t1, t0, ctx->mem_idx, MO_LEUL);
        break;
    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;

    }

    gen_set_gpr(rd, t1);
    tcg_temp_free(t0);
    tcg_temp_free(t1);
}


static void gen_store(DisasContext *ctx, uint32_t opc,
                      int rs1, int rs2, int16_t imm)
{
    target_long uimm = (target_long)imm; /* sign ext 16->64 bits */

    TCGv t0 = tcg_temp_new();
    TCGv dat = tcg_temp_new();
    gen_get_gpr(t0, rs1);
    tcg_gen_addi_tl(t0, t0, uimm);
    gen_get_gpr(dat, rs2);

    switch (opc) {
    case OPC_RISC_SB:
        tcg_gen_qemu_st_tl(dat, t0, ctx->mem_idx, MO_UB);
        break;
    case OPC_RISC_SH:
        tcg_gen_qemu_st_tl(dat, t0, ctx->mem_idx, MO_LEUW);
        break;
    case OPC_RISC_SW:
        tcg_gen_qemu_st_tl(dat, t0, ctx->mem_idx, MO_LEUL);
        break;
    case OPC_RISC_SD:
        tcg_gen_qemu_st_tl(dat, t0, ctx->mem_idx, MO_LEQ);
        break;

    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;
    }

    tcg_temp_free(t0);
    tcg_temp_free(dat);
}

static void gen_jalr(DisasContext *ctx, uint32_t opc, int rd, int rs1,
                     int16_t imm)
{
    TCGLabel *misaligned = gen_new_label();
    TCGLabel *done = gen_new_label();
    target_long uimm = (target_long)imm; /* sign ext 16->64 bits */
    TCGv t0, t1, t2, t3;
    t0 = tcg_temp_local_new();
    t1 = tcg_temp_local_new();
    t2 = tcg_temp_local_new(); /* old_pc */
    t3 = tcg_temp_local_new();

    switch (opc) {

    case OPC_RISC_JALR: /* CANNOT HAVE CHAINING WITH JALR */
        gen_get_gpr(t0, rs1);
        tcg_gen_addi_tl(t0, t0, uimm);
        tcg_gen_andi_tl(t0, t0, 0xFFFFFFFFFFFFFFFEll);

        tcg_gen_andi_tl(t3, t0, 0x2);
        tcg_gen_movi_tl(t2, ctx->pc);

        tcg_gen_brcondi_tl(TCG_COND_NE, t3, 0x0, misaligned);
        tcg_gen_mov_tl(cpu_PC, t0);
        tcg_gen_addi_tl(t1, t2, 4);
        gen_set_gpr(rd, t1);
        tcg_gen_br(done);

        gen_set_label(misaligned);
        /* generate_exception_mbadaddr(ctx, NEW_RISCV_EXCP_INST_ADDR_MIS); */

        gen_set_label(done);
        tcg_gen_exit_tb(0); /* exception or not, NO CHAINING FOR JALR */
        ctx->bstate = BS_BRANCH;
        break;
    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;

    }
    tcg_temp_free(t0);
    tcg_temp_free(t1);
    tcg_temp_free(t2);
    tcg_temp_free(t3);

}

static void gen_atomic(DisasContext *ctx, uint32_t opc,
                       int rd, int rs1, int rs2)
{
    /* TODO: handle aq, rl bits? - for now just get rid of them: */
    opc = MASK_OP_ATOMIC_NO_AQ_RL(opc);

    TCGv source1, source2, dat;

    source1 = tcg_temp_local_new();
    source2 = tcg_temp_local_new();
    dat = tcg_temp_new();

    gen_get_gpr(source1, rs1);
    gen_get_gpr(source2, rs2);

    switch (opc) {
        /* all currently implemented as non-atomics */
    case OPC_RISC_LR_W:
        /* put addr in load_reservation */
        tcg_gen_mov_tl(load_reservation, source1);
        tcg_gen_qemu_ld32s(source1, source1, ctx->mem_idx);
        break;
    case OPC_RISC_SC_W: {
        TCGLabel *fail = gen_new_label();
        TCGLabel *done = gen_new_label();
        tcg_gen_brcond_tl(TCG_COND_NE, load_reservation, source1, fail);
        tcg_gen_qemu_st32(source2, source1, ctx->mem_idx);
        tcg_gen_movi_tl(source1, 0); /*success */
        tcg_gen_br(done);
        gen_set_label(fail);
        tcg_gen_movi_tl(source1, 1); /*fail */
        gen_set_label(done);
        }
        break;
    case OPC_RISC_AMOSWAP_W:
        tcg_gen_qemu_ld_tl(dat, source1, ctx->mem_idx, MO_LESL);
        tcg_gen_qemu_st_tl(source2, source1, ctx->mem_idx, MO_LEUL);
        tcg_gen_mov_tl(source1, dat);
        break;
    case OPC_RISC_AMOADD_W:
        tcg_gen_qemu_ld_tl(dat, source1, ctx->mem_idx, MO_LESL);
        tcg_gen_add_tl(source2, dat, source2);
        tcg_gen_qemu_st_tl(source2, source1, ctx->mem_idx, MO_LEUL);
        tcg_gen_mov_tl(source1, dat);
        break;
    case OPC_RISC_AMOXOR_W:
        tcg_gen_qemu_ld_tl(dat, source1, ctx->mem_idx, MO_LESL);
        tcg_gen_xor_tl(source2, dat, source2);
        tcg_gen_qemu_st_tl(source2, source1, ctx->mem_idx, MO_LEUL);
        tcg_gen_mov_tl(source1, dat);
        break;
    case OPC_RISC_AMOAND_W:
        tcg_gen_qemu_ld_tl(dat, source1, ctx->mem_idx, MO_LESL);
        tcg_gen_and_tl(source2, dat, source2);
        tcg_gen_qemu_st_tl(source2, source1, ctx->mem_idx, MO_LEUL);
        tcg_gen_mov_tl(source1, dat);
        break;
    case OPC_RISC_AMOOR_W:
        tcg_gen_qemu_ld_tl(dat, source1, ctx->mem_idx, MO_LESL);
        tcg_gen_or_tl(source2, dat, source2);
        tcg_gen_qemu_st_tl(source2, source1, ctx->mem_idx, MO_LEUL);
        tcg_gen_mov_tl(source1, dat);
        break;
    case OPC_RISC_AMOMIN_W:
        {
            TCGv source1_l, source2_l, dat_l;
            source1_l = tcg_temp_local_new();
            source2_l = tcg_temp_local_new();
            dat_l = tcg_temp_local_new();
            TCGLabel *j = gen_new_label();
            TCGLabel *done = gen_new_label();
            tcg_gen_mov_tl(source1_l, source1);
            tcg_gen_ext32s_tl(source2_l, source2);
            tcg_gen_qemu_ld_tl(dat_l, source1_l, ctx->mem_idx, MO_LESL);
            tcg_gen_brcond_tl(TCG_COND_LT, dat_l, source2_l, j);
            tcg_gen_qemu_st_tl(source2_l, source1_l, ctx->mem_idx, MO_LEUL);
            tcg_gen_br(done);
            /* here we store the thing on the left */
            gen_set_label(j);
            tcg_gen_qemu_st_tl(dat_l, source1_l, ctx->mem_idx, MO_LEUL);
            /*done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, dat_l);
            tcg_temp_free(source1_l);
            tcg_temp_free(source2_l);
            tcg_temp_free(dat_l);
        }
        break;
    case OPC_RISC_AMOMAX_W:
        {
            TCGv source1_l, source2_l, dat_l;
            source1_l = tcg_temp_local_new();
            source2_l = tcg_temp_local_new();
            dat_l = tcg_temp_local_new();
            TCGLabel *j = gen_new_label();
            TCGLabel *done = gen_new_label();
            tcg_gen_mov_tl(source1_l, source1);
            tcg_gen_ext32s_tl(source2_l, source2);
            tcg_gen_qemu_ld_tl(dat_l, source1_l, ctx->mem_idx, MO_LESL);
            tcg_gen_brcond_tl(TCG_COND_GT, dat_l, source2_l, j);
            tcg_gen_qemu_st_tl(source2_l, source1_l, ctx->mem_idx, MO_LEUL);
            tcg_gen_br(done);
            /* here we store the thing on the left */
            gen_set_label(j);
            tcg_gen_qemu_st_tl(dat_l, source1_l, ctx->mem_idx, MO_LEUL);
            /*done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, dat_l);
            tcg_temp_free(source1_l);
            tcg_temp_free(source2_l);
            tcg_temp_free(dat_l);
        }
        break;
    case OPC_RISC_AMOMINU_W:
        {
            TCGv source1_l, source2_l, dat_l;
            source1_l = tcg_temp_local_new();
            source2_l = tcg_temp_local_new();
            dat_l = tcg_temp_local_new();
            TCGLabel *j = gen_new_label();
            TCGLabel *done = gen_new_label();
            tcg_gen_mov_tl(source1_l, source1);
            tcg_gen_ext32u_tl(source2_l, source2);
            tcg_gen_qemu_ld_tl(dat_l, source1_l, ctx->mem_idx, MO_LEUL);
            tcg_gen_brcond_tl(TCG_COND_LTU, dat_l, source2_l, j);
            tcg_gen_qemu_st_tl(source2_l, source1_l, ctx->mem_idx, MO_LEUL);
            tcg_gen_br(done);
            /* here we store the thing on the left */
            gen_set_label(j);
            tcg_gen_qemu_st_tl(dat_l, source1_l, ctx->mem_idx, MO_LEUL);
            /*done */
            gen_set_label(done);
            tcg_gen_ext32s_tl(source1, dat_l);
            tcg_temp_free(source1_l);
            tcg_temp_free(source2_l);
            tcg_temp_free(dat_l);
        }
        break;
    case OPC_RISC_AMOMAXU_W:
        {
            TCGv source1_l, source2_l, dat_l;
            source1_l = tcg_temp_local_new();
            source2_l = tcg_temp_local_new();
            dat_l = tcg_temp_local_new();
            TCGLabel *j = gen_new_label();
            TCGLabel *done = gen_new_label();
            tcg_gen_mov_tl(source1_l, source1);
            tcg_gen_ext32u_tl(source2_l, source2);
            tcg_gen_qemu_ld_tl(dat_l, source1_l, ctx->mem_idx, MO_LEUL);
            tcg_gen_brcond_tl(TCG_COND_GTU, dat_l, source2_l, j);
            tcg_gen_qemu_st_tl(source2_l, source1_l, ctx->mem_idx, MO_LEUL);
            tcg_gen_br(done);
            /* here we store the thing on the left */
            gen_set_label(j);
            tcg_gen_qemu_st_tl(dat_l, source1_l, ctx->mem_idx, MO_LEUL);
            /*done */
            gen_set_label(done);
            tcg_gen_ext32s_tl(source1, dat_l);
            tcg_temp_free(source1_l);
            tcg_temp_free(source2_l);
            tcg_temp_free(dat_l);
        }
        break;
    case OPC_RISC_LR_D:
        /* put addr in load_reservation */
        tcg_gen_mov_tl(load_reservation, source1);
        tcg_gen_qemu_ld64(source1, source1, ctx->mem_idx);
        break;
    case OPC_RISC_SC_D: {
        TCGLabel *fail = gen_new_label();
        TCGLabel *done = gen_new_label();
        tcg_gen_brcond_tl(TCG_COND_NE, load_reservation, source1, fail);
        tcg_gen_qemu_st_tl(source2, source1, ctx->mem_idx, MO_LEQ);
        tcg_gen_movi_tl(source1, 0); /*success */
        tcg_gen_br(done);
        gen_set_label(fail);
        tcg_gen_movi_tl(source1, 1); /*fail */
        gen_set_label(done);
        break;
        }
    case OPC_RISC_AMOSWAP_D:
        tcg_gen_qemu_ld64(dat, source1, ctx->mem_idx);
        tcg_gen_qemu_st_tl(source2, source1, ctx->mem_idx, MO_LEQ);
        tcg_gen_mov_tl(source1, dat);
        break;
    case OPC_RISC_AMOADD_D:
        tcg_gen_qemu_ld64(dat, source1, ctx->mem_idx);
        tcg_gen_add_tl(source2, dat, source2);
        tcg_gen_qemu_st_tl(source2, source1, ctx->mem_idx, MO_LEQ);
        tcg_gen_mov_tl(source1, dat);
        break;
    case OPC_RISC_AMOXOR_D:
        tcg_gen_qemu_ld64(dat, source1, ctx->mem_idx);
        tcg_gen_xor_tl(source2, dat, source2);
        tcg_gen_qemu_st_tl(source2, source1, ctx->mem_idx, MO_LEQ);
        tcg_gen_mov_tl(source1, dat);
        break;
    case OPC_RISC_AMOAND_D:
        tcg_gen_qemu_ld64(dat, source1, ctx->mem_idx);
        tcg_gen_and_tl(source2, dat, source2);
        tcg_gen_qemu_st_tl(source2, source1, ctx->mem_idx, MO_LEQ);
        tcg_gen_mov_tl(source1, dat);
        break;
    case OPC_RISC_AMOOR_D:
        tcg_gen_qemu_ld64(dat, source1, ctx->mem_idx);
        tcg_gen_or_tl(source2, dat, source2);
        tcg_gen_qemu_st_tl(source2, source1, ctx->mem_idx, MO_LEQ);
        tcg_gen_mov_tl(source1, dat);
        break;
    case OPC_RISC_AMOMIN_D:
        {
            TCGv source1_l, source2_l, dat_l;
            source1_l = tcg_temp_local_new();
            source2_l = tcg_temp_local_new();
            dat_l = tcg_temp_local_new();
            TCGLabel *j = gen_new_label();
            TCGLabel *done = gen_new_label();
            tcg_gen_mov_tl(source1_l, source1);
            tcg_gen_mov_tl(source2_l, source2);
            tcg_gen_qemu_ld64(dat_l, source1_l, ctx->mem_idx);
            tcg_gen_brcond_tl(TCG_COND_LT, dat_l, source2_l, j);
            tcg_gen_qemu_st_tl(source2_l, source1_l, ctx->mem_idx, MO_LEQ);
            tcg_gen_br(done);
            /* here we store the thing on the left */
            gen_set_label(j);
            tcg_gen_qemu_st_tl(dat_l, source1_l, ctx->mem_idx, MO_LEQ);
            /*done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, dat_l);
            tcg_temp_free(source1_l);
            tcg_temp_free(source2_l);
            tcg_temp_free(dat_l);
        }
        break;
    case OPC_RISC_AMOMAX_D:
        {
            TCGv source1_l, source2_l, dat_l;
            source1_l = tcg_temp_local_new();
            source2_l = tcg_temp_local_new();
            dat_l = tcg_temp_local_new();
            TCGLabel *j = gen_new_label();
            TCGLabel *done = gen_new_label();
            tcg_gen_mov_tl(source1_l, source1);
            tcg_gen_mov_tl(source2_l, source2);
            tcg_gen_qemu_ld64(dat_l, source1_l, ctx->mem_idx);
            tcg_gen_brcond_tl(TCG_COND_GT, dat_l, source2_l, j);
            tcg_gen_qemu_st64(source2_l, source1_l, ctx->mem_idx);
            tcg_gen_br(done);
            /* here we store the thing on the left */
            gen_set_label(j);
            tcg_gen_qemu_st_tl(dat_l, source1_l, ctx->mem_idx, MO_LEQ);
            /*done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, dat_l);
            tcg_temp_free(source1_l);
            tcg_temp_free(source2_l);
            tcg_temp_free(dat_l);
        }
        break;
    case OPC_RISC_AMOMINU_D:
        {
            TCGv source1_l, source2_l, dat_l;
            source1_l = tcg_temp_local_new();
            source2_l = tcg_temp_local_new();
            dat_l = tcg_temp_local_new();
            TCGLabel *j = gen_new_label();
            TCGLabel *done = gen_new_label();
            tcg_gen_mov_tl(source1_l, source1);
            tcg_gen_mov_tl(source2_l, source2);
            tcg_gen_qemu_ld64(dat_l, source1_l, ctx->mem_idx);
            tcg_gen_brcond_tl(TCG_COND_LTU, dat_l, source2_l, j);
            tcg_gen_qemu_st_tl(source2_l, source1_l, ctx->mem_idx, MO_LEQ);
            tcg_gen_br(done);
            /* here we store the thing on the left */
            gen_set_label(j);
            tcg_gen_qemu_st_tl(dat_l, source1_l, ctx->mem_idx, MO_LEQ);
            /*done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, dat_l);
            tcg_temp_free(source1_l);
            tcg_temp_free(source2_l);
            tcg_temp_free(dat_l);
        }
        break;
    case OPC_RISC_AMOMAXU_D:
        {
            TCGv source1_l, source2_l, dat_l;
            source1_l = tcg_temp_local_new();
            source2_l = tcg_temp_local_new();
            dat_l = tcg_temp_local_new();
            TCGLabel *j = gen_new_label();
            TCGLabel *done = gen_new_label();
            tcg_gen_mov_tl(source1_l, source1);
            tcg_gen_mov_tl(source2_l, source2);
            tcg_gen_qemu_ld64(dat_l, source1_l, ctx->mem_idx);
            tcg_gen_brcond_tl(TCG_COND_GTU, dat_l, source2_l, j);
            tcg_gen_qemu_st_tl(source2_l, source1_l, ctx->mem_idx, MO_LEQ);
            tcg_gen_br(done);
            /* here we store the thing on the left */
            gen_set_label(j);
            tcg_gen_qemu_st_tl(dat_l, source1_l, ctx->mem_idx, MO_LEQ);
            /*done */
            gen_set_label(done);
            tcg_gen_mov_tl(source1, dat_l);
            tcg_temp_free(source1_l);
            tcg_temp_free(source2_l);
            tcg_temp_free(dat_l);
        }
        break;
    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;

    }

    /* set and free */
    gen_set_gpr(rd, source1);
    tcg_temp_free(source1);
    tcg_temp_free(source2);
    tcg_temp_free(dat);
}

static void gen_fp_load(DisasContext *ctx, uint32_t opc,
                      int rd, int rs1, int16_t imm)
{
    target_long uimm = (target_long)imm; /* sign ext 16->64 bits */

    TCGv t0 = tcg_temp_new();
    gen_get_gpr(t0, rs1);
    tcg_gen_addi_tl(t0, t0, uimm);

    switch (opc) {

    case OPC_RISC_FLW:
        tcg_gen_qemu_ld_tl(t0, t0, ctx->mem_idx, MO_LEUL);
        break;
    case OPC_RISC_FLD:
        tcg_gen_qemu_ld_tl(t0, t0, ctx->mem_idx, MO_LEQ);
        break;
    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;

    }
    tcg_gen_mov_tl(cpu_fpr[rd], t0); /* probably can just get rid of this and
                                        store directly to cpu_fpr[rd] */
    tcg_temp_free(t0);
}

static void gen_fp_store(DisasContext *ctx, uint32_t opc,
                      int rs1, int rs2, int16_t imm)
{
    target_long uimm = (target_long)imm; /* sign ext 16->64 bits */

    TCGv t0 = tcg_temp_new();
    TCGv dat = tcg_temp_new();
    gen_get_gpr(t0, rs1);
    tcg_gen_addi_tl(t0, t0, uimm);
    tcg_gen_mov_tl(dat, cpu_fpr[rs2]);

    switch (opc) {

    case OPC_RISC_FSW:
        tcg_gen_qemu_st_tl(dat, t0, ctx->mem_idx, MO_LEUL);
        break;
    case OPC_RISC_FSD:
        tcg_gen_qemu_st_tl(dat, t0, ctx->mem_idx, MO_LEQ);
        break;

    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;
    }

    tcg_temp_free(t0);
    tcg_temp_free(dat);
}

static void gen_fp_fmadd(DisasContext *ctx, uint32_t opc,
                         int rd, int rs1, int rs2, int rs3, int rm)
{
    TCGv rm_reg = tcg_temp_new();
    tcg_gen_movi_tl(rm_reg, rm);

    switch (opc) {

    case OPC_RISC_FMADD_S:
        gen_helper_fmadd_s(cpu_fpr[rd], cpu_env, cpu_fpr[rs1], cpu_fpr[rs2],
                           cpu_fpr[rs3], rm_reg);
        break;
    case OPC_RISC_FMADD_D:
        gen_helper_fmadd_d(cpu_fpr[rd], cpu_env, cpu_fpr[rs1], cpu_fpr[rs2],
                           cpu_fpr[rs3], rm_reg);
        break;
    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;
    }
    tcg_temp_free(rm_reg);

}

static void gen_fp_fmsub(DisasContext *ctx, uint32_t opc,
                         int rd, int rs1, int rs2, int rs3, int rm)
{
    TCGv rm_reg = tcg_temp_new();
    tcg_gen_movi_tl(rm_reg, rm);

    switch (opc) {

    case OPC_RISC_FMSUB_S:
        gen_helper_fmsub_s(cpu_fpr[rd], cpu_env, cpu_fpr[rs1], cpu_fpr[rs2],
                           cpu_fpr[rs3], rm_reg);
        break;
    case OPC_RISC_FMSUB_D:
        gen_helper_fmsub_d(cpu_fpr[rd], cpu_env, cpu_fpr[rs1], cpu_fpr[rs2],
                           cpu_fpr[rs3], rm_reg);
        break;
    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;
    }
    tcg_temp_free(rm_reg);
}

static void decode_opc(CPURISCVState *env, DisasContext *ctx)
{
    int rs1;
    int rs2;
    int rd;
    uint32_t op;
    int16_t imm;
    target_long ubimm;

    /* do not do misaligned address check here, address should never be
       misaligned

       instead, all control flow instructions check for themselves

       this is because epc must be the address of the control flow instruction
       that "caused" to the misaligned instruction access

        we leave this check here for now, since not all control flow
        instructions have been updated yet */

    /* make sure instructions are on a word boundary */
    if (unlikely(ctx->pc & 0x3)) {
        printf("addr misaligned\n");
        printf("misaligned instruction, not completely implemented for\
                riscv\n");
        exit(1);
        return;
    }

    op = MASK_OP_MAJOR(ctx->opcode);
    rs1 = (ctx->opcode >> 15) & 0x1f;
    rs2 = (ctx->opcode >> 20) & 0x1f;
    rd = (ctx->opcode >> 7) & 0x1f;
    imm = (int16_t)(((int32_t)ctx->opcode) >> 20); /* sign extends */

#ifdef RISCV_DEBUG_PRINT
    /* this will print a log similar to spike, should be left off unless
       you're debugging QEMU */
    int start = 1; /*0 && ctx->pc == 0x8ccac; */
    TCGv print_helper_tmp = tcg_temp_local_new();
    TCGv printpc = tcg_temp_local_new();
    tcg_gen_movi_tl(print_helper_tmp, ctx->opcode);
    tcg_gen_movi_tl(printpc, ctx->pc);

    if (monitor_region || start) {
        gen_helper_debug_print(cpu_env, printpc, print_helper_tmp);
        monitor_region = 1;

        /* can print some reg val too */
        gen_helper_debug_print(cpu_env, cpu_fpr[28], cpu_fpr[28]);

    }
    tcg_temp_free(print_helper_tmp);
    tcg_temp_free(printpc);
#endif

    switch (op) {
    case OPC_RISC_LUI:
        if (rd == 0) {
            break; /* NOP */
        }
        tcg_gen_movi_tl(cpu_gpr[rd], (ctx->opcode & 0xFFFFF000));
        tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
        break;
    case OPC_RISC_AUIPC:
        if (rd == 0) {
            break; /* NOP */
        }
        tcg_gen_movi_tl(cpu_gpr[rd], (ctx->opcode & 0xFFFFF000));
        tcg_gen_ext32s_tl(cpu_gpr[rd], cpu_gpr[rd]);
        tcg_gen_add_tl(cpu_gpr[rd], cpu_gpr[rd], tcg_const_tl(ctx->pc));
        break;
    case OPC_RISC_JAL: {
            TCGv nextpc = tcg_temp_local_new();
            TCGv testpc = tcg_temp_local_new();
            TCGLabel *misaligned = gen_new_label();
            TCGLabel *done = gen_new_label();
            ubimm = (target_long) (GET_JAL_IMM(ctx->opcode));
            tcg_gen_movi_tl(nextpc, ctx->pc + ubimm);
            /* check misaligned: */
            tcg_gen_andi_tl(testpc, nextpc, 0x3);
            tcg_gen_brcondi_tl(TCG_COND_NE, testpc, 0x0, misaligned);
            if (rd != 0) {
                tcg_gen_movi_tl(cpu_gpr[rd], 4);
                tcg_gen_addi_tl(cpu_gpr[rd], cpu_gpr[rd], ctx->pc);
            }

            gen_goto_tb(ctx, 0, ctx->pc + ubimm); /* must use this for safety */

            tcg_gen_br(done);
            gen_set_label(misaligned);
            /* TODO: throw exception for misaligned case */
            gen_set_label(done);
            ctx->bstate = BS_BRANCH;
            tcg_temp_free(nextpc);
            tcg_temp_free(testpc);
        }
        break;
    case OPC_RISC_JALR:
        gen_jalr(ctx, MASK_OP_JALR(ctx->opcode), rd, rs1, imm);
        break;
    case OPC_RISC_BRANCH:
        gen_branch(ctx, MASK_OP_BRANCH(ctx->opcode), rs1, rs2,
                   GET_B_IMM(ctx->opcode));
        break;
    case OPC_RISC_LOAD:
        gen_load(ctx, MASK_OP_LOAD(ctx->opcode), rd, rs1, imm);
        break;
    case OPC_RISC_STORE:
        gen_store(ctx, MASK_OP_STORE(ctx->opcode), rs1, rs2,
                  GET_STORE_IMM(ctx->opcode));
        break;
    case OPC_RISC_ARITH_IMM:
        if (rd == 0) {
            break; /* NOP */
        }
        gen_arith_imm(ctx, MASK_OP_ARITH_IMM(ctx->opcode), rd, rs1, imm);
        break;
    case OPC_RISC_ARITH:
        if (rd == 0) {
            break; /* NOP */
        }
        gen_arith(ctx, MASK_OP_ARITH(ctx->opcode), rd, rs1, rs2);
        break;
    case OPC_RISC_ARITH_IMM_W:
        if (rd == 0) {
            break; /* NOP */
        }
        gen_arith_imm_w(ctx, MASK_OP_ARITH_IMM_W(ctx->opcode), rd, rs1, imm);
        break;
    case OPC_RISC_ARITH_W:
        if (rd == 0) {
            break; /* NOP */
        }
        gen_arith_w(ctx, MASK_OP_ARITH_W(ctx->opcode), rd, rs1, rs2);
        break;
    case OPC_RISC_ATOMIC:
        gen_atomic(ctx, MASK_OP_ATOMIC(ctx->opcode), rd, rs1, rs2);
        break;
    case OPC_RISC_FP_LOAD:
        gen_fp_load(ctx, MASK_OP_FP_LOAD(ctx->opcode), rd, rs1, imm);
        break;
    case OPC_RISC_FP_STORE:
        gen_fp_store(ctx, MASK_OP_FP_STORE(ctx->opcode), rs1, rs2,
                     GET_STORE_IMM(ctx->opcode));
        break;
    case OPC_RISC_FMADD:
        gen_fp_fmadd(ctx, MASK_OP_FP_FMADD(ctx->opcode), rd, rs1, rs2,
                     GET_RS3(ctx->opcode), GET_RM(ctx->opcode));
        break;
    case OPC_RISC_FMSUB:
        gen_fp_fmsub(ctx, MASK_OP_FP_FMSUB(ctx->opcode), rd, rs1, rs2,
                     GET_RS3(ctx->opcode), GET_RM(ctx->opcode));
        break;
    default:
        kill_unknown(ctx, NEW_RISCV_EXCP_ILLEGAL_INST);
        break;
    }
}



void gen_intermediate_code(CPURISCVState *env, TranslationBlock *tb)
{
    RISCVCPU *cpu = riscv_env_get_cpu(env);
    CPUState *cs = CPU(cpu);
    DisasContext ctx;
    target_ulong pc_start;
    target_ulong next_page_start; /* new */
    int num_insns;
    int max_insns;
    pc_start = tb->pc;
    next_page_start = (pc_start & TARGET_PAGE_MASK) + TARGET_PAGE_SIZE;
    ctx.pc = pc_start;

    /* once we have GDB, the rest of the translate.c implementation should be */
    /* ready for singlestep */
    ctx.singlestep_enabled = cs->singlestep_enabled;

    ctx.tb = tb;
    ctx.bstate = BS_NONE;

    /* restore_cpu_state? */

    ctx.mem_idx = cpu_mmu_index(env, false);
    num_insns = 0;
    max_insns = tb->cflags & CF_COUNT_MASK;
    if (max_insns == 0) {
        max_insns = CF_COUNT_MASK;
    }
    if (max_insns > TCG_MAX_INSNS) {
        max_insns = TCG_MAX_INSNS;
    }
    gen_tb_start(tb);

    while (ctx.bstate == BS_NONE) {
        tcg_gen_insn_start(ctx.pc);
        num_insns++;

        if (num_insns == max_insns && (tb->cflags & CF_LAST_IO)) {
            gen_io_start();
        }

        ctx.opcode = cpu_ldl_code(env, ctx.pc);
        decode_opc(env, &ctx);
        ctx.pc += 4;

        if (cs->singlestep_enabled) {
            break;
        }
        if (ctx.pc >= next_page_start) {
            break;
        }
        if (tcg_op_buf_full()) {
            break;
        }
        if (num_insns >= max_insns) {
            break;
        }
        if (singlestep) {
            break;
        }

    }
    if (tb->cflags & CF_LAST_IO) {
        gen_io_end();
    }

    switch (ctx.bstate) {
    case BS_STOP:
        gen_goto_tb(&ctx, 0, ctx.pc);
        break;
    case BS_NONE:
        /* DO NOT CHAIN. This is for END-OF-PAGE. See gen_goto_tb. */
        tcg_gen_movi_tl(cpu_PC, ctx.pc); /* NOT PC+4, that was already done */
        tcg_gen_exit_tb(0);
        break;
    case BS_BRANCH:
        /* anything using BS_BRANCH will have generated it's own exit seq */
    default:
        break;
    }

    gen_tb_end(tb, num_insns);
    tb->size = ctx.pc - pc_start;
    tb->icount = num_insns;

}


void riscv_cpu_dump_state(CPUState *cs, FILE *f, fprintf_function cpu_fprintf,
                         int flags)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    int i;

    cpu_fprintf(f, "pc=0x" TARGET_FMT_lx "\n", env->PC);
    for (i = 0; i < 32; i++) {
        cpu_fprintf(f, " %s " TARGET_FMT_lx, regnames[i], env->gpr[i]);
        if ((i & 3) == 3) {
            cpu_fprintf(f, "\n");
        }
    }

    cpu_fprintf(f, " %s " TARGET_FMT_lx "\n", "MSTATUS ",
                env->csr[NEW_CSR_MSTATUS]);
    cpu_fprintf(f, " %s " TARGET_FMT_lx "\n", "MTIMECMP",
                env->csr[NEW_CSR_MTIMECMP]);

    cpu_fprintf(f, " %s " TARGET_FMT_lx "\n", "MIP     ",
                env->csr[NEW_CSR_MIP]);

    cpu_fprintf(f, " %s " TARGET_FMT_lx "\n", "MIE     ",
                env->csr[NEW_CSR_MIE]);

    for (i = 0; i < 32; i++) {
        if ((i & 3) == 0) {
            cpu_fprintf(f, "FPR%02d:", i);
        }
        cpu_fprintf(f, " %s " TARGET_FMT_lx, fpr_regnames[i], env->fpr[i]);
        if ((i & 3) == 3) {
            cpu_fprintf(f, "\n");
        }
    }
}

void riscv_tcg_init(void)
{
    int i;
    static int inited;

    /* Initialize various static tables. */
    if (inited) {
        return;
    }

    cpu_env = tcg_global_reg_new_ptr(TCG_AREG0, "env");

    /* WARNING: cpu_gpr[0] is not allocated ON PURPOSE. Do not use it. */
    /* Use the gen_set_gpr and gen_get_gpr helper functions when accessing */
    /* registers, unless you specifically block reads/writes to reg 0 */
    TCGV_UNUSED(cpu_gpr[0]);
    for (i = 1; i < 32; i++) {
        cpu_gpr[i] = tcg_global_mem_new(cpu_env,
                                        offsetof(CPURISCVState, gpr[i]),
                                        regnames[i]);
    }

    for (i = 0; i < 32; i++) {
        cpu_fpr[i] = tcg_global_mem_new(cpu_env,
                                        offsetof(CPURISCVState, fpr[i]),
                                        fpr_regnames[i]);
    }

    cpu_PC = tcg_global_mem_new(cpu_env,
                                offsetof(CPURISCVState, PC), "PC");

    /* TODO: not initialized */
    load_reservation = tcg_global_mem_new(cpu_env,
                       offsetof(CPURISCVState, load_reservation),
                       "load_reservation");
    inited = 1;
}

#define MCPUID_RV64I   (2L << 62)
#define MCPUID_SUPER   (1L << ('S' - 'A'))
#define MCPUID_USER    (1L << ('U' - 'A'))
#define MCPUID_I       (1L << ('I' - 'A'))
#define MCPUID_M       (1L << ('M' - 'A'))
#define MCPUID_A       (1L << ('A' - 'A'))
#define MCPUID_F       (1L << ('F' - 'A'))
#define MCPUID_D       (1L << ('D' - 'A'))

struct riscv_def_t {
    const char *name;
    target_ulong init_mcpuid_reg;
};

/* RISC-V CPU definitions */
static const riscv_def_t riscv_defs[] = {
    {
        .name = "riscv-generic",
        /* for now, hardcode RV64G: */
        .init_mcpuid_reg = MCPUID_RV64I | MCPUID_SUPER | /*MCPUID_USER |*/
                           MCPUID_I | MCPUID_M | MCPUID_A | MCPUID_F | MCPUID_D,
    },
};

static const riscv_def_t *cpu_riscv_find_by_name(const char *name)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(riscv_defs); i++) {
        if (strcasecmp(name, riscv_defs[i].name) == 0) {
            return &riscv_defs[i];
        }
    }
    return NULL;
}

void riscv_cpu_list(FILE *f, fprintf_function cpu_fprintf)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(riscv_defs); i++) {
        (*cpu_fprintf)(f, "RISCV '%s'\n", riscv_defs[i].name);
    }
}

RISCVCPU *cpu_riscv_init(const char *cpu_model)
{
    RISCVCPU *cpu;
    CPURISCVState *env;
    const riscv_def_t *def;

    def = cpu_riscv_find_by_name(cpu_model);
    if (!def) {
        return NULL;
    }
    cpu = RISCV_CPU(object_new(TYPE_RISCV_CPU));
    env = &cpu->env;
    env->cpu_model = def;

    memset(env->csr, 0, 4096 * sizeof(uint64_t));

    /* init mstatus */
    target_ulong start_mstatus = 0;
    start_mstatus = set_field(start_mstatus, MSTATUS_PRV, PRV_M);
    start_mstatus = set_field(start_mstatus, MSTATUS_PRV1, PRV_U);
    start_mstatus = set_field(start_mstatus, MSTATUS_PRV2, PRV_U);
    env->csr[NEW_CSR_MSTATUS] = start_mstatus;

    /* set mcpuid from def */
    env->csr[NEW_CSR_MCPUID] = def->init_mcpuid_reg;
    object_property_set_bool(OBJECT(cpu), true, "realized", NULL);

    set_default_nan_mode(1, &env->fp_status);

    return cpu;
}

void cpu_state_reset(CPURISCVState *env)
{
    RISCVCPU *cpu = riscv_env_get_cpu(env);
    CPUState *cs = CPU(cpu);

    env->PC = RISCV_START_PC; /* STARTING PC VALUE def'd in cpu.h */
    cs->exception_index = EXCP_NONE;
}

void restore_state_to_opc(CPURISCVState *env, TranslationBlock *tb,
                          target_ulong *data)
{
    env->PC = data[0];
}
