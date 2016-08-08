/*
 *  RISC-V emulation helpers for qemu.
 *
 *  Author: Sagar Karandikar, sagark@eecs.berkeley.edu
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

#if !defined(CONFIG_USER_ONLY)

/* get_physical_address - get the physical address for this virtual address
 *
 * Do a page table walk to obtain the physical address corresponding to a
 * virtual address.
 *
 * Returns 0 if the translation was successful
*/
static int get_physical_address(CPURISCVState *env, hwaddr *physical,
                               int *prot, target_ulong address,
                                MMUAccessType access_type, int mmu_idx)
{
    /* NOTE: the env->PC value visible here will not be
     * correct, but the value visible to the exception handler
     * (riscv_cpu_do_interrupt) is correct */

    /* access_type is either MMU_INST_FETCH, MMU_DATA_STORE, or MMU_DATA_LOAD */
    *prot = 0;
    CPUState *cs = CPU(riscv_env_get_cpu(env));

    target_ulong mode = get_field(env->csr[NEW_CSR_MSTATUS], MSTATUS_PRV);
    if (access_type != MMU_INST_FETCH && get_field(env->csr[NEW_CSR_MSTATUS],
                                                   MSTATUS_MPRV)) {
        mode = get_field(env->csr[NEW_CSR_MSTATUS], MSTATUS_PRV1);
    }
    if (get_field(env->csr[NEW_CSR_MSTATUS], MSTATUS_VM) == VM_MBARE) {
        mode = PRV_M;
    }

    /* check to make sure that mmu_idx and mode that we get matches */
    if (unlikely(mode != mmu_idx)) {
        fprintf(stderr, "MODE, mmu_idx mismatch\n");
        exit(1);
    }

    if (mode == PRV_M) {
        target_ulong msb_mask = (2UL << 63) - 1;
        *physical = address & msb_mask;
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return TRANSLATE_SUCCESS;
    }

    target_ulong addr = address;
    int supervisor = mode > PRV_U;

    int levels, ptidxbits, ptesize;
    switch (get_field(env->csr[NEW_CSR_MSTATUS], MSTATUS_VM)) {
    case VM_SV32:
        printf("currently unsupported SV32\n");
        exit(1);
        levels = 2;
        ptidxbits = 10;
        ptesize = 4;
        break;
    case VM_SV39:
        levels = 3;
        ptidxbits = 9;
        ptesize = 8;
        break;
    case VM_SV48:
        levels = 4;
        ptidxbits = 9;
        ptesize = 8;
        break;
    default:
        printf("unsupported MSTATUS_VM value\n");
        exit(1);
    }

    int va_bits = PGSHIFT + levels * ptidxbits;
    target_ulong mask = (1L << (64 - (va_bits - 1))) - 1;
    target_ulong masked_msbs = (addr >> (va_bits - 1)) & mask;
    if (masked_msbs != 0 && masked_msbs != mask) {
        return TRANSLATE_FAIL;
    }

    target_ulong base = env->csr[NEW_CSR_SPTBR];
    int ptshift = (levels - 1) * ptidxbits;
    int i;
    for (i = 0; i < levels; i++, ptshift -= ptidxbits) {
        target_ulong idx = (addr >> (PGSHIFT + ptshift))
                           & ((1 << ptidxbits) - 1);

        /* check that physical address of PTE is legal */
        target_ulong pte_addr = base + idx * ptesize;
        if (pte_addr >= env->memsize) {
            break;
        }

        target_ulong pte = ldq_phys(cs->as, pte_addr);
        target_ulong ppn = pte >> PTE_PPN_SHIFT;

        if (PTE_TABLE(pte)) { /* next level of page table */
            base = ppn << PGSHIFT;
        } else if (!PTE_CHECK_PERM(pte, supervisor,
                    access_type == MMU_DATA_STORE,
                    access_type == MMU_INST_FETCH)) {
            break;
        } else {
            /* set referenced and possibly dirty bits. */
            /* we only put it in the TLB if it has the right stuff */
            stq_phys(cs->as, pte_addr, ldq_phys(cs->as, pte_addr) | PTE_R |
                    ((access_type == MMU_DATA_STORE) * PTE_D));

            /* for superpage mappings, make a fake leaf PTE for the TLB's
               benefit. */
            target_ulong vpn = addr >> PGSHIFT;
            *physical = (ppn | (vpn & ((1L << ptshift) - 1))) << PGSHIFT;

            /* we do not give all prots indicated by the PTE
               this is because future accesses need to do things like set the
               dirty bit on the PTE */
            if (supervisor) {
                if (PTE_SX(pte) && access_type == MMU_INST_FETCH) {
                    *prot |= PAGE_EXEC;
                } else if (PTE_SW(pte) && access_type == MMU_DATA_STORE) {
                    *prot |= PAGE_WRITE;
                } else if (PTE_SR(pte) && access_type == MMU_DATA_LOAD) {
                    *prot |= PAGE_READ;
                } else {
                    printf("err in translation prots");
                    exit(1);
                }
            } else {
                if (PTE_UX(pte) && access_type == MMU_INST_FETCH) {
                    *prot |= PAGE_EXEC;
                } else if (PTE_UW(pte) && access_type == MMU_DATA_STORE) {
                    *prot |= PAGE_WRITE;
                } else if (PTE_UR(pte) && access_type == MMU_DATA_LOAD) {
                    *prot |= PAGE_READ;
                } else {
                    printf("err in translation prots");
                    exit(1);
                }
            }
            return TRANSLATE_SUCCESS;
        }
    }
    return TRANSLATE_FAIL;
}
#endif

static void raise_mmu_exception(CPURISCVState *env, target_ulong address,
                                MMUAccessType access_type)
{
    CPUState *cs = CPU(riscv_env_get_cpu(env));
    int exception = 0;
    if (access_type == MMU_INST_FETCH) { /* inst access */
        exception = NEW_RISCV_EXCP_INST_ACCESS_FAULT;
        env->csr[NEW_CSR_MBADADDR] = address;
    } else if (access_type == MMU_DATA_STORE) { /* store access */
        exception = NEW_RISCV_EXCP_STORE_AMO_ACCESS_FAULT;
        env->csr[NEW_CSR_MBADADDR] = address;
    } else if (access_type == MMU_DATA_LOAD) { /* load access */
        exception = NEW_RISCV_EXCP_LOAD_ACCESS_FAULT;
        env->csr[NEW_CSR_MBADADDR] = address;
    } else {
        fprintf(stderr, "FAIL: invalid access_type\n");
        exit(1);
    }
    cs->exception_index = exception;
}

#if !defined(CONFIG_USER_ONLY)
hwaddr riscv_cpu_get_phys_page_debug(CPUState *cs, vaddr addr)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    hwaddr phys_addr;
    int prot;
    int mem_idx = cpu_mmu_index(&cpu->env, false);

    if (get_physical_address(&cpu->env, &phys_addr, &prot, addr, 0, mem_idx)) {
        return -1;
    }
    return phys_addr;
}
#endif

/* This is called when there is no QEMU "TLB" match
 *
 * Assuming system mode, only called in target-riscv/op_helper:tlb_fill
 */
int riscv_cpu_handle_mmu_fault(CPUState *cs, vaddr address,
                               MMUAccessType access_type, int mmu_idx)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    hwaddr physical;
    physical = 0; /* stop gcc complaining */
    int prot;
    int ret = 0;

    qemu_log("%s pc " TARGET_FMT_lx " ad %" VADDR_PRIx
              " access_type %d mmu_idx %d\n",
              __func__, env->PC, address, access_type, mmu_idx);

    ret = get_physical_address(env, &physical, &prot, address, access_type,
                               mmu_idx);
    qemu_log("%s address=%" VADDR_PRIx " ret %d physical " TARGET_FMT_plx
             " prot %d\n",
             __func__, address, ret, physical, prot);
    if (ret == TRANSLATE_SUCCESS) {
        tlb_set_page(cs, address & TARGET_PAGE_MASK, physical & TARGET_PAGE_MASK
                     , prot, mmu_idx, TARGET_PAGE_SIZE);
    } else if (ret == TRANSLATE_FAIL) {
        raise_mmu_exception(env, address, access_type);
    }
    return ret;
}

target_ulong push_priv_stack(target_ulong start_mstatus)
{
    target_ulong s = start_mstatus;
    s = set_field(s, MSTATUS_PRV2, get_field(start_mstatus, MSTATUS_PRV1));
    s = set_field(s, MSTATUS_IE2, get_field(start_mstatus, MSTATUS_IE1));
    s = set_field(s, MSTATUS_PRV1, get_field(start_mstatus, MSTATUS_PRV));
    s = set_field(s, MSTATUS_IE1, get_field(start_mstatus, MSTATUS_IE));
    s = set_field(s, MSTATUS_PRV, PRV_M);
    s = set_field(s, MSTATUS_MPRV, 0);
    s = set_field(s, MSTATUS_IE, 0);
    return s;
}

target_ulong pop_priv_stack(target_ulong start_mstatus)
{
    target_ulong s = start_mstatus;
    s = set_field(s, MSTATUS_PRV, get_field(start_mstatus, MSTATUS_PRV1));
    s = set_field(s, MSTATUS_IE, get_field(start_mstatus, MSTATUS_IE1));
    s = set_field(s, MSTATUS_PRV1, get_field(start_mstatus, MSTATUS_PRV2));
    s = set_field(s, MSTATUS_IE1, get_field(start_mstatus, MSTATUS_IE2));
    s = set_field(s, MSTATUS_PRV2, PRV_U);
    s = set_field(s, MSTATUS_IE2, 1);
    return s;
}
