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
