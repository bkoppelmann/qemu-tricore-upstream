/*
 * ARM gdb server stub
 *
 * Copyright (c) 2003-2005 Fabrice Bellard
 * Copyright (c) 2013 SUSE LINUX Products GmbH
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
#include "qemu-common.h"
#include "exec/gdbstub.h"


#define LCX_REGNUM                      32 /* lcx     */
#define FCX_REGNUM                      33 /* fcx     */
#define PCXI_REGNUM                     34 /* pcxi    */
#define TRICORE_PSW_REGNUM              35 /* psw     */
#define TRICORE_PC_REGNUM               36 /* pc      */
#define ICR_REGNUM                      37 /* icr     */
#define ISP_REGNUM                      38 /* isp     */
#define BTV_REGNUM                      39 /* btv     */
#define BIV_REGNUM                      40 /* biv     */
#define SYSCON_REGNUM                   41 /* syscon  */
#define PMUCON0_REGNUM                  42 /* pmucon0 */
#define DMUCON_REGNUM                   43 /* dmucon  */

uint32_t tricore_cpu_gdb_read_csfr(CPUTriCoreState *env, int n)
{
    switch (n) {
        case LCX_REGNUM:
            return env->LCX;
        case FCX_REGNUM:
            return env->FCX;
        case PCXI_REGNUM:
            return env->PCXI;
        case TRICORE_PSW_REGNUM:
            return psw_read(env);
        case TRICORE_PC_REGNUM:
            return env->PC;
        case ICR_REGNUM:
            return env->ICR;
        case ISP_REGNUM:
            return env->ISP;
        case BTV_REGNUM:
            return env->BTV;
        case BIV_REGNUM:
            return env->BIV;
        case SYSCON_REGNUM:
            return env->SYSCON;
        case PMUCON0_REGNUM:
            return 0; /* PMUCON0 */
        case DMUCON_REGNUM:
            return 0; /* DMUCON0 */
        default:
            return 0;
    }
}

int tricore_cpu_gdb_read_register(CPUState *cs, uint8_t *mem_buf, int n)
{
    TriCoreCPU *cpu = TRICORE_CPU(cs);
    CPUTriCoreState *env = &cpu->env;

    if (n < 16) { /* data registers? */
        return gdb_get_reg32(mem_buf, env->gpr_d[n]);
    } else if (n < 32) {
        return gdb_get_reg32(mem_buf, env->gpr_a[n-16]);
    } else {
        return gdb_get_reg32(mem_buf, tricore_cpu_gdb_read_csfr(env, n));
    }
    printf("tricore_cpu_gdb_read_register\n");
    return 0;
}

int tricore_cpu_gdb_write_register(CPUState *cs, uint8_t *mem_buf, int n)
{
    printf("tricore_cpu_gdb_write_register\n");
    exit(0);
    return 0;
}
