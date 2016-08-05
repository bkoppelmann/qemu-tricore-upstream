/*
 *  RISC-V emulation helpers for qemu.
 *
 *  Author: Sagar Karandikar, sagark@eecs.berkeley.edu
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
/* MULHSU helper */
DEF_HELPER_3(mulhsu, tl, env, tl, tl)
/* Floating Point - fused */
DEF_HELPER_5(fmadd_s, tl, env, tl, tl, tl, tl)
DEF_HELPER_5(fmadd_d, tl, env, tl, tl, tl, tl)
DEF_HELPER_5(fmsub_s, tl, env, tl, tl, tl, tl)
DEF_HELPER_5(fmsub_d, tl, env, tl, tl, tl, tl)
DEF_HELPER_5(fnmsub_s, tl, env, tl, tl, tl, tl)
DEF_HELPER_5(fnmsub_d, tl, env, tl, tl, tl, tl)
DEF_HELPER_5(fnmadd_s, tl, env, tl, tl, tl, tl)
DEF_HELPER_5(fnmadd_d, tl, env, tl, tl, tl, tl)

/* Floating Point - Single Precision */
DEF_HELPER_4(fadd_s, tl, env, tl, tl, tl)
DEF_HELPER_4(fsub_s, tl, env, tl, tl, tl)
DEF_HELPER_4(fmul_s, tl, env, tl, tl, tl)
DEF_HELPER_4(fdiv_s, tl, env, tl, tl, tl)
DEF_HELPER_3(fsgnj_s, tl, env, tl, tl)
DEF_HELPER_3(fsgnjn_s, tl, env, tl, tl)
DEF_HELPER_3(fsgnjx_s, tl, env, tl, tl)
DEF_HELPER_3(fmin_s, tl, env, tl, tl)
DEF_HELPER_3(fmax_s, tl, env, tl, tl)
DEF_HELPER_3(fsqrt_s, tl, env, tl, tl)
DEF_HELPER_3(fle_s, tl, env, tl, tl)
DEF_HELPER_3(flt_s, tl, env, tl, tl)
DEF_HELPER_3(feq_s, tl, env, tl, tl)
DEF_HELPER_3(fcvt_w_s, tl, env, tl, tl)
DEF_HELPER_3(fcvt_wu_s, tl, env, tl, tl)
DEF_HELPER_3(fcvt_l_s, tl, env, tl, tl)
DEF_HELPER_3(fcvt_lu_s, tl, env, tl, tl)
DEF_HELPER_3(fcvt_s_w, tl, env, tl, tl)
DEF_HELPER_3(fcvt_s_wu, tl, env, tl, tl)
DEF_HELPER_3(fcvt_s_l, tl, env, tl, tl)
DEF_HELPER_3(fcvt_s_lu, tl, env, tl, tl)
DEF_HELPER_2(fclass_s, tl, env, tl)

/* Special functions */
#ifndef CONFIG_USER_ONLY
DEF_HELPER_3(debug_print, void, env, tl, tl)
#endif /* !CONFIG_USER_ONLY */
