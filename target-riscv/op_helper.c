/*
 * RISC-V Emulation Helpers for QEMU.
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
#include "qemu/host-utils.h"
#include "exec/helper-proto.h"

static int validate_vm(target_ulong vm)
{
    return vm == VM_SV39 || vm == VM_SV48 || vm == VM_MBARE;
}

static int validate_priv(target_ulong priv)
{
    return priv == PRV_U || priv == PRV_S || priv == PRV_M;
}

/* Exceptions processing helpers */
static inline void QEMU_NORETURN do_raise_exception_err(CPURISCVState *env,
                                          uint32_t exception, uintptr_t pc)
{
    CPUState *cs = CPU(riscv_env_get_cpu(env));
    qemu_log("%s: %d\n", __func__, exception);
    cs->exception_index = exception;
    cpu_loop_exit_restore(cs, pc);
}

void helper_raise_exception(CPURISCVState *env, uint32_t exception)
{
    /* TODO which one? */
    do_raise_exception_err(env, exception, 0);
    /* do_raise_exception_err(env, exception, GETPC()); */
}

void helper_raise_exception_debug(CPURISCVState *env)
{
    do_raise_exception_err(env, EXCP_DEBUG, 0);
}


void helper_raise_exception_err(CPURISCVState *env, uint32_t exception,
                                target_ulong pc)
{
    do_raise_exception_err(env, exception, pc);
}

void helper_raise_exception_mbadaddr(CPURISCVState *env, uint32_t exception,
        target_ulong bad_pc) {
    env->csr[NEW_CSR_MBADADDR] = bad_pc;
    do_raise_exception_err(env, exception, 0);
}

/*
 * Handle writes to CSRs and any resulting special behavior
 *
 * Note: mtohost and mfromhost are not handled here
 */
inline void csr_write_helper(CPURISCVState *env, target_ulong val_to_write,
        target_ulong csrno)
{
    #ifdef RISCV_DEBUG_PRINT
    fprintf(stderr, "Write CSR reg: 0x" TARGET_FMT_lx "\n", csrno);
    fprintf(stderr, "Write CSR val: 0x" TARGET_FMT_lx "\n", val_to_write);
    #endif

    switch (csrno) {
    case NEW_CSR_FFLAGS:
        env->csr[NEW_CSR_MSTATUS] |= MSTATUS_FS | MSTATUS64_SD;
        env->csr[NEW_CSR_FFLAGS] = val_to_write & (FSR_AEXC >> FSR_AEXC_SHIFT);
        break;
    case NEW_CSR_FRM:
        env->csr[NEW_CSR_MSTATUS] |= MSTATUS_FS | MSTATUS64_SD;
        env->csr[NEW_CSR_FRM] = val_to_write & (FSR_RD >> FSR_RD_SHIFT);
        break;
    case NEW_CSR_FCSR:
        env->csr[NEW_CSR_MSTATUS] |= MSTATUS_FS | MSTATUS64_SD;
        env->csr[NEW_CSR_FFLAGS] = (val_to_write & FSR_AEXC) >> FSR_AEXC_SHIFT;
        env->csr[NEW_CSR_FRM] = (val_to_write & FSR_RD) >> FSR_RD_SHIFT;
        break;
    case NEW_CSR_MTIME:
    case NEW_CSR_STIMEW:
        /* this implementation ignores writes to MTIME */
        break;
    case NEW_CSR_MTIMEH:
    case NEW_CSR_STIMEHW:
        /* this implementation ignores writes to MTIME */
        break;
    case NEW_CSR_TIMEW:
        /* cpu_riscv_store_timew(env, val_to_write); */
        break;
    case NEW_CSR_TIMEHW:
        fprintf(stderr, "CSR_TIMEHW unsupported on RV64I\n");
        exit(1);
        break;
    case NEW_CSR_CYCLEW:
    case NEW_CSR_INSTRETW:
        /* cpu_riscv_store_instretw(env, val_to_write); */
        break;
    case NEW_CSR_CYCLEHW:
    case NEW_CSR_INSTRETHW:
        fprintf(stderr, "CSR_CYCLEHW/INSTRETHW unsupported on RV64I\n");
        exit(1);
        break;
    case NEW_CSR_MSTATUS: {
        target_ulong mstatus = env->csr[NEW_CSR_MSTATUS];
        #ifdef RISCV_DEBUG_PRINT
        target_ulong debug_mstatus = mstatus;
        #endif
        if ((val_to_write ^ mstatus) &
                (MSTATUS_VM | MSTATUS_PRV | MSTATUS_PRV1 | MSTATUS_MPRV)) {
            #ifdef RISCV_DEBUG_PRINT
            fprintf(stderr, "flushing TLB\n");
            #endif
            helper_tlb_flush(env);
        }

        /* no extension support */
        target_ulong mask = MSTATUS_IE | MSTATUS_IE1 | MSTATUS_IE2
            | MSTATUS_MPRV | MSTATUS_FS;

        if (validate_vm(get_field(val_to_write, MSTATUS_VM))) {
            mask |= MSTATUS_VM;
        }
        if (validate_priv(get_field(val_to_write, MSTATUS_PRV))) {
            mask |= MSTATUS_PRV;
        }
        if (validate_priv(get_field(val_to_write, MSTATUS_PRV1))) {
            mask |= MSTATUS_PRV1;
        }
        if (validate_priv(get_field(val_to_write, MSTATUS_PRV2))) {
            mask |= MSTATUS_PRV2;
        }

        mstatus = (mstatus & ~mask) | (val_to_write & mask);

        int dirty = (mstatus & MSTATUS_FS) == MSTATUS_FS;
        dirty |= (mstatus & MSTATUS_XS) == MSTATUS_XS;
        mstatus = set_field(mstatus, MSTATUS64_SD, dirty);
        env->csr[NEW_CSR_MSTATUS] = mstatus;
        break;
    }
    case NEW_CSR_MIP: {
        target_ulong mask = MIP_SSIP | MIP_MSIP | MIP_STIP;
        env->csr[NEW_CSR_MIP] = (env->csr[NEW_CSR_MIP] & ~mask) |
            (val_to_write & mask);
        CPUState *cs = CPU(riscv_env_get_cpu(env));
        if (env->csr[NEW_CSR_MIP] & MIP_SSIP) {
            stw_phys(cs->as, 0xFFFFFFFFF0000020, 0x1);
        } else {
            stw_phys(cs->as, 0xFFFFFFFFF0000020, 0x0);
        }
        if (env->csr[NEW_CSR_MIP] & MIP_STIP) {
            stw_phys(cs->as, 0xFFFFFFFFF0000040, 0x1);
        } else {
            stw_phys(cs->as, 0xFFFFFFFFF0000040, 0x0);
        }
        if (env->csr[NEW_CSR_MIP] & MIP_MSIP) {
            stw_phys(cs->as, 0xFFFFFFFFF0000060, 0x1);
        } else {
            stw_phys(cs->as, 0xFFFFFFFFF0000060, 0x0);
        }
        break;
    }
    case NEW_CSR_MIPI: {
        CPUState *cs = CPU(riscv_env_get_cpu(env));
        env->csr[NEW_CSR_MIP] = set_field(env->csr[NEW_CSR_MIP], MIP_MSIP,
                                          val_to_write & 1);
        if (env->csr[NEW_CSR_MIP] & MIP_MSIP) {
            stw_phys(cs->as, 0xFFFFFFFFF0000060, 0x1);
        } else {
            stw_phys(cs->as, 0xFFFFFFFFF0000060, 0x0);
        }
        break;
    }
    case NEW_CSR_MIE: {
        target_ulong mask = MIP_SSIP | MIP_MSIP | MIP_STIP | MIP_MTIP;
        env->csr[NEW_CSR_MIE] = (env->csr[NEW_CSR_MIE] & ~mask) |
            (val_to_write & mask);
        break;
    }
    case NEW_CSR_SSTATUS: {
        target_ulong ms = env->csr[NEW_CSR_MSTATUS];
        ms = set_field(ms, MSTATUS_IE, get_field(val_to_write, SSTATUS_IE));
        ms = set_field(ms, MSTATUS_IE1, get_field(val_to_write, SSTATUS_PIE));
        ms = set_field(ms, MSTATUS_PRV1, get_field(val_to_write, SSTATUS_PS));
        ms = set_field(ms, MSTATUS_FS, get_field(val_to_write, SSTATUS_FS));
        ms = set_field(ms, MSTATUS_XS, get_field(val_to_write, SSTATUS_XS));
        ms = set_field(ms, MSTATUS_MPRV, get_field(val_to_write, SSTATUS_MPRV));
        csr_write_helper(env, ms, NEW_CSR_MSTATUS);
        break;
    }
    case NEW_CSR_SIP: {
        target_ulong mask = MIP_SSIP;
        env->csr[NEW_CSR_MIP] = (env->csr[NEW_CSR_MIP] & ~mask) |
            (val_to_write & mask);
        CPUState *cs = CPU(riscv_env_get_cpu(env));
        if (env->csr[NEW_CSR_MIP] & MIP_SSIP) {
            stw_phys(cs->as, 0xFFFFFFFFF0000020, 0x1);
        } else {
            stw_phys(cs->as, 0xFFFFFFFFF0000020, 0x0);
        }
        break;
    }
    case NEW_CSR_SIE: {
        target_ulong mask = MIP_SSIP | MIP_STIP;
        env->csr[NEW_CSR_MIE] = (env->csr[NEW_CSR_MIE] & ~mask) |
            (val_to_write & mask);
        break;
    }
    case NEW_CSR_SEPC:
        env->csr[NEW_CSR_SEPC] = val_to_write;
        break;
    case NEW_CSR_STVEC:
        env->csr[NEW_CSR_STVEC] = val_to_write >> 2 << 2;
        break;
    case NEW_CSR_SPTBR:
        env->csr[NEW_CSR_SPTBR] = val_to_write & -(1L << PGSHIFT);
        break;
    case NEW_CSR_SSCRATCH:
        env->csr[NEW_CSR_SSCRATCH] = val_to_write;
        break;
    case NEW_CSR_MEPC:
        env->csr[NEW_CSR_MEPC] = val_to_write;
        break;
    case NEW_CSR_MSCRATCH:
        env->csr[NEW_CSR_MSCRATCH] = val_to_write;
        break;
    case NEW_CSR_MCAUSE:
        env->csr[NEW_CSR_MCAUSE] = val_to_write;
        break;
    case NEW_CSR_MBADADDR:
        env->csr[NEW_CSR_MBADADDR] = val_to_write;
        break;
    case NEW_CSR_MTIMECMP:
        /* NOTE: clearing bit in MIP handled in cpu_riscv_store_compare */
        /* cpu_riscv_store_compare(env, val_to_write); */
        break;
    case NEW_CSR_MTOHOST:
        fprintf(stderr, "Write to mtohost should not be handled here\n");
        exit(1);
        break;
    case NEW_CSR_MFROMHOST:
        fprintf(stderr, "Write to mfromhost should not be handled here\n");
        exit(1);
        break;
    }
}

/*
 * Handle reads to CSRs and any resulting special behavior
 *
 * Note: mtohost and mfromhost are not handled here
 */
inline target_ulong csr_read_helper(CPURISCVState *env, target_ulong csrno)
{
    int csrno2 = (int)csrno;
    #ifdef RISCV_DEBUG_PRINT
    fprintf(stderr, "READ CSR 0x%x\n", csrno2);
    #endif

    switch (csrno2) {
    case NEW_CSR_FFLAGS:
        return env->csr[NEW_CSR_FFLAGS];
    case NEW_CSR_FRM:
        return env->csr[NEW_CSR_FRM];
    case NEW_CSR_FCSR:
        return (env->csr[NEW_CSR_FFLAGS] << FSR_AEXC_SHIFT) |
               (env->csr[NEW_CSR_FRM] << FSR_RD_SHIFT);
    case NEW_CSR_MTIME:
        /* return cpu_riscv_read_mtime(env); */
        return 0;
    case NEW_CSR_STIME:
    case NEW_CSR_STIMEW:
        /* return cpu_riscv_read_stime(env); */
        return 0;
    case NEW_CSR_MTIMEH:
    case NEW_CSR_STIMEH:
    case NEW_CSR_STIMEHW:
        fprintf(stderr, "CSR_MTIMEH unsupported on RV64I\n");
        exit(1);
    case NEW_CSR_TIME:
    case NEW_CSR_TIMEW:
        /* return cpu_riscv_read_time(env); */
        return 0;
    case NEW_CSR_CYCLE:
    case NEW_CSR_CYCLEW:
    case NEW_CSR_INSTRET:
    case NEW_CSR_INSTRETW:
        /* return cpu_riscv_read_instretw(env); */
        return 0;
    case NEW_CSR_TIMEH:
    case NEW_CSR_TIMEHW:
        fprintf(stderr, "CSR_TIMEH unsupported on RV64I\n");
        exit(1);
    case NEW_CSR_CYCLEH:
    case NEW_CSR_INSTRETH:
    case NEW_CSR_CYCLEHW:
    case NEW_CSR_INSTRETHW:
        fprintf(stderr, "CSR_INSTRETH unsupported on RV64I\n");
        exit(1);
    case NEW_CSR_SSTATUS: {
        target_ulong ss = 0;
        ss = set_field(ss, SSTATUS_IE, get_field(env->csr[NEW_CSR_MSTATUS],
                    MSTATUS_IE));
        ss = set_field(ss, SSTATUS_PIE, get_field(env->csr[NEW_CSR_MSTATUS],
                    MSTATUS_IE1));
        ss = set_field(ss, SSTATUS_PS, get_field(env->csr[NEW_CSR_MSTATUS],
                    MSTATUS_PRV1));
        ss = set_field(ss, SSTATUS_FS, get_field(env->csr[NEW_CSR_MSTATUS],
                    MSTATUS_FS));
        ss = set_field(ss, SSTATUS_XS, get_field(env->csr[NEW_CSR_MSTATUS],
                    MSTATUS_XS));
        ss = set_field(ss, SSTATUS_MPRV, get_field(env->csr[NEW_CSR_MSTATUS],
                    MSTATUS_MPRV));
        if (get_field(env->csr[NEW_CSR_MSTATUS], MSTATUS64_SD)) {
            ss = set_field(ss, SSTATUS64_SD, 1);
        }
        return ss;
    }
    case NEW_CSR_SIP:
        return env->csr[NEW_CSR_MIP] & (MIP_SSIP | MIP_STIP);
    case NEW_CSR_SIE:
        return env->csr[NEW_CSR_MIE] & (MIP_SSIP | MIP_STIP);
    case NEW_CSR_SEPC:
        return env->csr[NEW_CSR_SEPC];
    case NEW_CSR_SBADADDR:
        return env->csr[NEW_CSR_SBADADDR];
    case NEW_CSR_STVEC:
        return env->csr[NEW_CSR_STVEC];
    case NEW_CSR_SCAUSE:
        return env->csr[NEW_CSR_SCAUSE];
    case NEW_CSR_SPTBR:
        return env->csr[NEW_CSR_SPTBR];
    case NEW_CSR_SASID:
        return 0;
    case NEW_CSR_SSCRATCH:
        return env->csr[NEW_CSR_SSCRATCH];
    case NEW_CSR_MSTATUS:
        return env->csr[NEW_CSR_MSTATUS];
    case NEW_CSR_MIP:
        return env->csr[NEW_CSR_MIP];
    case NEW_CSR_MIPI:
        return 0;
    case NEW_CSR_MIE:
        return env->csr[NEW_CSR_MIE];
    case NEW_CSR_MEPC:
        return env->csr[NEW_CSR_MEPC];
    case NEW_CSR_MSCRATCH:
        return env->csr[NEW_CSR_MSCRATCH];
    case NEW_CSR_MCAUSE:
        return env->csr[NEW_CSR_MCAUSE];
    case NEW_CSR_MBADADDR:
        return env->csr[NEW_CSR_MBADADDR];
    case NEW_CSR_MTIMECMP:
        return env->csr[NEW_CSR_MTIMECMP];
    case NEW_CSR_MCPUID:
        return env->csr[NEW_CSR_MCPUID];
    case NEW_CSR_MIMPID:
        return 0x1; /* "Rocket" */
    case NEW_CSR_MHARTID:
        /* TODO: multi-hart */
        return 0;
    case NEW_CSR_MTVEC:
        return DEFAULT_MTVEC;
    case NEW_CSR_MTDELEG:
        return 0;
    case NEW_CSR_MTOHOST:
        fprintf(stderr, "Read from mtohost should not be handled here\n");
        exit(1);
    case NEW_CSR_MFROMHOST:
        fprintf(stderr, "Read from mfromhost should not be handled here\n");
        exit(1);
    case NEW_CSR_MIOBASE:
        return env->memsize;
    case NEW_CSR_UARCH0:
    case NEW_CSR_UARCH1:
    case NEW_CSR_UARCH2:
    case NEW_CSR_UARCH3:
    case NEW_CSR_UARCH4:
    case NEW_CSR_UARCH5:
    case NEW_CSR_UARCH6:
    case NEW_CSR_UARCH7:
    case NEW_CSR_UARCH8:
    case NEW_CSR_UARCH9:
    case NEW_CSR_UARCH10:
    case NEW_CSR_UARCH11:
    case NEW_CSR_UARCH12:
    case NEW_CSR_UARCH13:
    case NEW_CSR_UARCH14:
    case NEW_CSR_UARCH15:
        return 0;
    }
    fprintf(stderr, "Attempt to read invalid csr!\n");
    exit(1);
}

void validate_csr(CPURISCVState *env, uint64_t which, uint64_t write,
        uint64_t new_pc) {
    unsigned my_priv = get_field(env->csr[NEW_CSR_MSTATUS], MSTATUS_PRV);
    unsigned csr_priv = get_field((which), 0x300);
    unsigned csr_read_only = get_field((which), 0xC00) == 3;
    if (((write) && csr_read_only) || (my_priv < csr_priv)) {
        do_raise_exception_err(env, NEW_RISCV_EXCP_ILLEGAL_INST, new_pc);
    }
    return;
}

target_ulong helper_csrrw(CPURISCVState *env, target_ulong src,
        target_ulong csr, target_ulong new_pc)
{
    validate_csr(env, csr, 1, new_pc);
    uint64_t csr_backup = csr_read_helper(env, csr);
    csr_write_helper(env, src, csr);
    return csr_backup;
}

target_ulong helper_csrrs(CPURISCVState *env, target_ulong src,
        target_ulong csr, target_ulong new_pc)
{
    validate_csr(env, csr, src != 0, new_pc);
    uint64_t csr_backup = csr_read_helper(env, csr);
    csr_write_helper(env, src | csr_backup, csr);
    return csr_backup;
}

/* match spike behavior for validate_csr write flag */
target_ulong helper_csrrsi(CPURISCVState *env, target_ulong src,
        target_ulong csr, target_ulong new_pc)
{
    validate_csr(env, csr, 1, new_pc);
    uint64_t csr_backup = csr_read_helper(env, csr);
    csr_write_helper(env, src | csr_backup, csr);
    return csr_backup;
}

target_ulong helper_csrrc(CPURISCVState *env, target_ulong src,
        target_ulong csr, target_ulong new_pc) {
    validate_csr(env, csr, 1, new_pc);
    uint64_t csr_backup = csr_read_helper(env, csr);
    csr_write_helper(env, (~src) & csr_backup, csr);
    return csr_backup;
}

/*
 * This is a debug print helper for printing trace.
 * Currently calls spike-dasm, so very slow.
 * Probably not useful unless you're debugging riscv-qemu
 */
void helper_debug_print(CPURISCVState *env, target_ulong cpu_pc_deb,
        target_ulong instruction)
{
    fprintf(stderr, ": core   0: 0x" TARGET_FMT_lx " (0x%08lx) %s",
            cpu_pc_deb, instruction, "DASM BAD RESULT\n");
}

target_ulong helper_sret(CPURISCVState *env, target_ulong cpu_pc_deb)
{
    target_ulong mstatus = env->csr[NEW_CSR_MSTATUS];
    if (!(get_field(mstatus, MSTATUS_PRV) >= PRV_S)) {
        /* TODO: real illegal instruction trap */
        printf("ILLEGAL INST");
        exit(1);
    }

    target_ulong retpc = 0;
    switch (get_field(mstatus, MSTATUS_PRV)) {
    case PRV_S:
        /* set PC val to sepc */
        retpc = env->csr[NEW_CSR_SEPC];
        break;
    case PRV_M:
        /* set PC val to mepc */
        retpc = env->csr[NEW_CSR_MEPC];
        break;
    default:
        /* TODO: illegal inst */
        printf("ILLEGAL INST");
        exit(1);
        break;
    }
    if (retpc & 0x3) {
        /* check for misaligned fetch */
        helper_raise_exception_mbadaddr(env, NEW_RISCV_EXCP_INST_ADDR_MIS,
                cpu_pc_deb);
        return cpu_pc_deb;
    }

    target_ulong next_mstatus = pop_priv_stack(env->csr[NEW_CSR_MSTATUS]);
    csr_write_helper(env, next_mstatus, NEW_CSR_MSTATUS);
    return retpc;
}

target_ulong helper_mrts(CPURISCVState *env, target_ulong curr_pc)
{
    target_ulong mstatus = env->csr[NEW_CSR_MSTATUS];
    if (!(get_field(mstatus, MSTATUS_PRV) >= PRV_M)) {
        /* TODO: real illegal instruction trap */
        printf("ILLEGAL INST");
        exit(1);
    }

    csr_write_helper(env, set_field(mstatus, MSTATUS_PRV, PRV_S),
            NEW_CSR_MSTATUS);
    env->csr[NEW_CSR_SBADADDR] = env->csr[NEW_CSR_MBADADDR];
    env->csr[NEW_CSR_SCAUSE] = env->csr[NEW_CSR_MCAUSE];
    env->csr[NEW_CSR_SEPC] = env->csr[NEW_CSR_MEPC];

    if (env->csr[NEW_CSR_STVEC] & 0x3) {
        helper_raise_exception_mbadaddr(env, NEW_RISCV_EXCP_INST_ADDR_MIS,
                curr_pc);
        return curr_pc;
    }
    return env->csr[NEW_CSR_STVEC];
}

target_ulong helper_mulhsu(CPURISCVState *env, target_ulong arg1,
                          target_ulong arg2)
{
    int64_t a = arg1;
    uint64_t b = arg2;
    return (int64_t)((__int128_t)a * b >> 64);
}

#ifndef CONFIG_USER_ONLY

void helper_fence_i(CPURISCVState *env)
{
    RISCVCPU *cpu = riscv_env_get_cpu(env);
    CPUState *cs = CPU(cpu);
    /* Flush QEMU's TLB */
    tlb_flush(cs, 1);
    /* ARM port seems to not know if this is okay inside a TB...
       But we need to do it */
    tb_flush(cs);
}

void helper_tlb_flush(CPURISCVState *env)
{
    RISCVCPU *cpu = riscv_env_get_cpu(env);
    tlb_flush(CPU(cpu), 1);
}

#endif

/* called by qemu's softmmu to fill the qemu tlb */
void tlb_fill(CPUState *cs, target_ulong addr, MMUAccessType access_type,
              int mmu_idx, uintptr_t retaddr)
{
    int ret;
    ret = riscv_cpu_handle_mmu_fault(cs, addr, access_type, mmu_idx);
    if (ret == TRANSLATE_FAIL) {
        RISCVCPU *cpu = RISCV_CPU(cs);
        CPURISCVState *env = &cpu->env;
        do_raise_exception_err(env, cs->exception_index, retaddr);
    }

}
