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

/* called by qemu's softmmu to fill the qemu tlb */
void tlb_fill(CPUState *cs, target_ulong addr, MMUAccessType access_type,
              int mmu_idx, uintptr_t retaddr)
{
    int ret;
    ret = riscv_cpu_handle_mmu_fault(cs, addr, access_type, mmu_idx);
    if (ret == TRANSLATE_FAIL) {
        /* TODO: do_raise_exception_err(env, cs->exception_index, retaddr); */
    }

}
