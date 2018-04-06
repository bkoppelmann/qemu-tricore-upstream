bool trans16_src_add(DisasContext *ctx, arg_src_add *a, uint16_t insn)
{
    gen_addi_d(cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s1_d], a->const4);
    return true;
}

bool trans16_src_add_a15(DisasContext *ctx, arg_src_add_a15 *a, uint16_t insn)
{
    gen_addi_d(cpu_gpr_d[a->s1_d], cpu_gpr_d[15], a->const4);
    return true;
}

bool trans16_src_add_15A(DisasContext *ctx, arg_src_add_15A *a, uint16_t insn)
{
    gen_addi_d(cpu_gpr_d[15], cpu_gpr_d[a->s1_d], a->const4);
    return true;
}

bool trans16_srr_add(DisasContext *ctx, arg_srr_add *a, uint16_t insn)
{
    gen_add_d(cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s2]);
    return true;
}

bool trans16_srr_add_A15(DisasContext *ctx, arg_srr_add_A15 *a, uint16_t insn)
{
    gen_add_d(cpu_gpr_d[a->s1_d], cpu_gpr_d[15], cpu_gpr_d[a->s2]);
    return true;
}

bool trans16_srr_add_15A(DisasContext *ctx, arg_srr_add_15A *a, uint16_t insn)
{
    gen_add_d(cpu_gpr_d[15], cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s2]);
    return true;
}

bool trans16_src_add_a(DisasContext *ctx, arg_src_add_a *a, uint16_t insn)
{
    tcg_gen_addi_tl(cpu_gpr_a[a->s1_d], cpu_gpr_a[a->s1_d], a->const4);
    return true;
}

bool trans16_srr_add_a(DisasContext *ctx, arg_srr_add_a *a, uint16_t insn)
{
    tcg_gen_add_tl(cpu_gpr_a[a->s1_d], cpu_gpr_a[a->s1_d], cpu_gpr_a[a->s2]);
    return true;
}

bool trans16_add_s(DisasContext *ctx, arg_add_s *a, uint16_t insn)
{
    gen_adds(cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s2]);
    return true;
}

bool trans16_addsc_a(DisasContext *ctx, arg_addsc_a *a, uint16_t insn)
{
    return false;
}

bool trans16_sc_and(DisasContext *ctx, arg_sc_and *a, uint16_t insn)
{
    tcg_gen_andi_tl(cpu_gpr_d[15], cpu_gpr_d[15], a->const8);
    return true;
}

bool trans16_srr_and(DisasContext *ctx, arg_srr_and *a, uint16_t insn)
{
    tcg_gen_and_tl(cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s2]);
    return true;
}

bool trans16_bisr(DisasContext *ctx, arg_bisr *a, uint16_t insn)
{
    gen_helper_1arg(bisr, a->const8 & 0xff);
    return true;
}

bool trans16_cadd(DisasContext *ctx, arg_cadd *a, uint16_t insn)
{
    gen_condi_add(TCG_COND_NE, cpu_gpr_d[a->s1_d], a->const4,
                  cpu_gpr_d[a->s1_d], cpu_gpr_d[15]);
    return true;
}

bool trans16_caddn(DisasContext *ctx, arg_caddn *a, uint16_t insn)
{
    gen_condi_add(TCG_COND_EQ, cpu_gpr_d[a->s1_d], a->const4,
                  cpu_gpr_d[a->s1_d], cpu_gpr_d[15]);
    return true;
}

bool trans16_call(DisasContext *ctx, arg_call *a, uint16_t insn)
{
    return false;
}

bool trans16_src_cmov(DisasContext *ctx, arg_src_cmov *a, uint16_t insn)
{
    TCGv temp = tcg_const_tl(0);
    TCGv temp2 = tcg_const_tl(a->const4);
    tcg_gen_movcond_tl(TCG_COND_NE, cpu_gpr_d[a->s1_d], cpu_gpr_d[15], temp,
                       temp2, cpu_gpr_d[a->s1_d]);
    tcg_temp_free(temp);
    tcg_temp_free(temp2);
    return true;
}

bool trans16_srr_cmov(DisasContext *ctx, arg_srr_cmov *a, uint16_t insn)
{
    TCGv temp = tcg_const_tl(0);
    tcg_gen_movcond_tl(TCG_COND_NE, cpu_gpr_d[a->s1_d], cpu_gpr_d[15], temp,
                       cpu_gpr_d[a->s2], cpu_gpr_d[a->s1_d]);
    tcg_temp_free(temp);
    return true;
}

bool trans16_src_cmovn(DisasContext *ctx, arg_src_cmovn *a, uint16_t insn)
{
    TCGv temp = tcg_const_tl(0);
    TCGv temp2 = tcg_const_tl(a->const4);
    tcg_gen_movcond_tl(TCG_COND_EQ, cpu_gpr_d[a->s1_d], cpu_gpr_d[15], temp,
                       temp2, cpu_gpr_d[a->s1_d]);
    tcg_temp_free(temp);
    tcg_temp_free(temp2);
    return true;
}

bool trans16_srr_cmovn(DisasContext *ctx, arg_srr_cmovn *a, uint16_t insn)
{
    TCGv temp = tcg_const_tl(0);
    tcg_gen_movcond_tl(TCG_COND_EQ, cpu_gpr_d[a->s1_d], cpu_gpr_d[15], temp,
                       cpu_gpr_d[a->s2], cpu_gpr_d[a->s1_d]);
    tcg_temp_free(temp);
    return true;
}

bool trans16_debug(DisasContext *ctx, arg_debug *a, uint16_t insn)
{
    return true;
}

bool trans16_src_eq(DisasContext *ctx, arg_src_eq *a, uint16_t insn)
{
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_gpr_d[15], cpu_gpr_d[a->s1_d],
                        a->const4);
    return true;
}

bool trans16_srr_eq(DisasContext *ctx, arg_srr_eq *a, uint16_t insn)
{
    tcg_gen_setcond_tl(TCG_COND_EQ, cpu_gpr_d[15], cpu_gpr_d[a->s1_d],
                       cpu_gpr_d[a->s2]);
    return true;
}

bool trans16_fret(DisasContext *ctx, arg_fret *a, uint16_t insn)
{
    if (!tricore_has_feature(ctx, TRICORE_FEATURE_16)) {
        return false;
    }
    gen_fret(ctx);
    return true;
}

bool trans16_j(DisasContext *ctx, arg_j *a, uint16_t insn)
{
    return false;
}

bool trans16_sbc_jeq(DisasContext *ctx, arg_sbc_jeq *a, uint16_t insn)
{
    return false;
}

bool trans16_sbr_jeq(DisasContext *ctx, arg_sbr_jeq *a, uint16_t insn)
{
    return false;
}

bool trans16_jgez(DisasContext *ctx, arg_jgez *a, uint16_t insn)
{
    return false;
}

bool trans16_jgtz(DisasContext *ctx, arg_jgtz *a, uint16_t insn)
{
    return false;
}

bool trans16_ji(DisasContext *ctx, arg_ji *a, uint16_t insn)
{
    return false;
}

bool trans16_jlez(DisasContext *ctx, arg_jlez *a, uint16_t insn)
{
    return false;
}

bool trans16_jltz(DisasContext *ctx, arg_jltz *a, uint16_t insn)
{
    return false;
}

bool trans16_sbc_jne(DisasContext *ctx, arg_sbc_jne *a, uint16_t insn)
{
    return false;
}

bool trans16_sbr_jne(DisasContext *ctx, arg_sbr_jne *a, uint16_t insn)
{
    return false;
}

bool trans16_sb_jnz(DisasContext *ctx, arg_sb_jnz *a, uint16_t insn)
{
    return false;
}

bool trans16_sbr_jnz(DisasContext *ctx, arg_sbr_jnz *a, uint16_t insn)
{
    return false;
}

bool trans16_jnz_a(DisasContext *ctx, arg_jnz_a *a, uint16_t insn)
{
    return false;
}

bool trans16_jnz_t(DisasContext *ctx, arg_jnz_t *a, uint16_t insn)
{
    return false;
}

bool trans16_sb_jz(DisasContext *ctx, arg_sb_jz *a, uint16_t insn)
{
    return false;
}

bool trans16_sbr_jz(DisasContext *ctx, arg_sbr_jz *a, uint16_t insn)
{
    return false;
}

bool trans16_jz_a(DisasContext *ctx, arg_jz_a *a, uint16_t insn)
{
    return false;
}

bool trans16_jz_t(DisasContext *ctx, arg_jz_t *a, uint16_t insn)
{
    return false;
}

bool trans16_sc_ld_a(DisasContext *ctx, arg_sc_ld_a *a, uint16_t insn)
{
    gen_offset_ld(ctx, cpu_gpr_a[15], cpu_gpr_a[10], a->const8 * 4, MO_LESL);
    return true;
}

bool trans16_slr_ld_a(DisasContext *ctx, arg_slr_ld_a *a, uint16_t insn)
{
    tcg_gen_qemu_ld_tl(cpu_gpr_a[a->d], cpu_gpr_a[a->s2], ctx->mem_idx, MO_LESL);
    return true;
}

bool trans16_slr_ld_a_pi(DisasContext *ctx, arg_slr_ld_a_pi *a, uint16_t insn)
{
    tcg_gen_qemu_ld_tl(cpu_gpr_a[a->d], cpu_gpr_a[a->s2], ctx->mem_idx, MO_LESL);
    tcg_gen_addi_tl(cpu_gpr_a[a->s2], cpu_gpr_a[a->s2], 4);
    return true;
}

bool trans16_slro_ld_a(DisasContext *ctx, arg_slro_ld_a *a, uint16_t insn)
{
    return false;
}

bool trans16_sro_ld_a(DisasContext *ctx, arg_sro_ld_a *a, uint16_t insn)
{
    gen_offset_ld(ctx, cpu_gpr_a[15], cpu_gpr_a[a->s2], a->off4 * 4, MO_LESL);
    return true;
}

bool trans16_slr_ld_bu(DisasContext *ctx, arg_slr_ld_bu *a, uint16_t insn)
{
    tcg_gen_qemu_ld_tl(cpu_gpr_d[a->d], cpu_gpr_a[a->s2], ctx->mem_idx, MO_UB);
    return true;
}

bool trans16_slr_ld_bu_pi(DisasContext *ctx, arg_slr_ld_bu_pi *a, uint16_t insn)
{
    tcg_gen_qemu_ld_tl(cpu_gpr_d[a->d], cpu_gpr_a[a->s2], ctx->mem_idx, MO_UB);
    tcg_gen_addi_tl(cpu_gpr_a[a->s2], cpu_gpr_a[a->s2], 1);
    return true;
}

bool trans16_slro_ld_bu(DisasContext *ctx, arg_slro_ld_bu *a, uint16_t insn)
{
    return false;
}

bool trans16_sro_ld_bu(DisasContext *ctx, arg_sro_ld_bu *a, uint16_t insn)
{
    gen_offset_ld(ctx, cpu_gpr_d[15], cpu_gpr_a[a->s2], a->off4, MO_UB);
    return true;
}

bool trans16_slr_ld_h(DisasContext *ctx, arg_slr_ld_h *a, uint16_t insn)
{
    tcg_gen_qemu_ld_tl(cpu_gpr_d[a->d], cpu_gpr_a[a->s2], ctx->mem_idx, MO_LESW);
    return true;
}

bool trans16_slr_ld_h_pi(DisasContext *ctx, arg_slr_ld_h_pi *a, uint16_t insn)
{
    tcg_gen_qemu_ld_tl(cpu_gpr_d[a->d], cpu_gpr_a[a->s2], ctx->mem_idx, MO_LESW);
    tcg_gen_addi_tl(cpu_gpr_a[a->s2], cpu_gpr_a[a->s2], 2);
    return true;
}

bool trans16_slro_ld_h(DisasContext *ctx, arg_slro_ld_h *a, uint16_t insn)
{
    return false;
}

bool trans16_sro_ld_h(DisasContext *ctx, arg_sro_ld_h *a, uint16_t insn)
{
    gen_offset_ld(ctx, cpu_gpr_d[15], cpu_gpr_a[a->s2], a->off4, MO_LESW);
    return true;
}

bool trans16_sc_ld_w(DisasContext *ctx, arg_sc_ld_w *a, uint16_t insn)
{
    gen_offset_ld(ctx, cpu_gpr_d[15], cpu_gpr_a[10], a->const8 * 4, MO_LESL);
    return true;
}

bool trans16_slr_ld_w(DisasContext *ctx, arg_slr_ld_w *a, uint16_t insn)
{
    tcg_gen_qemu_ld_tl(cpu_gpr_d[a->d], cpu_gpr_a[a->s2], ctx->mem_idx, MO_LESL);
    return true;
}

bool trans16_slr_ld_w_pi(DisasContext *ctx, arg_slr_ld_w_pi *a, uint16_t insn)
{
    tcg_gen_qemu_ld_tl(cpu_gpr_d[a->d], cpu_gpr_a[a->s2], ctx->mem_idx, MO_LESL);
    tcg_gen_addi_tl(cpu_gpr_a[a->s2], cpu_gpr_a[a->s2], 4);
    return true;
}

bool trans16_slro_ld_w(DisasContext *ctx, arg_slro_ld_w *a, uint16_t insn)
{
    return false;
}

bool trans16_sro_ld_w(DisasContext *ctx, arg_sro_ld_w *a, uint16_t insn)
{
    gen_offset_ld(ctx, cpu_gpr_d[15], cpu_gpr_a[a->s2], a->off4 * 4, MO_LESL);
    return true;
}

bool trans16_loop(DisasContext *ctx, arg_loop *a, uint16_t insn)
{
    return false;
}

bool trans16_src_lt(DisasContext *ctx, arg_src_lt *a, uint16_t insn)
{
    tcg_gen_setcondi_tl(TCG_COND_LT, cpu_gpr_d[15], cpu_gpr_d[a->s1_d],
                        a->const4);
    return true;
}

bool trans16_srr_lt(DisasContext *ctx, arg_srr_lt *a, uint16_t insn)
{
    tcg_gen_setcond_tl(TCG_COND_LT, cpu_gpr_d[15], cpu_gpr_d[a->s1_d],
                       cpu_gpr_d[a->s2]);
    return false;
}

bool trans16_sc_mov(DisasContext *ctx, arg_sc_mov *a, uint16_t insn)
{
    tcg_gen_movi_tl(cpu_gpr_d[15], a->const8);
    return true;
}

bool trans16_src_mov(DisasContext *ctx, arg_src_mov *a, uint16_t insn)
{
    tcg_gen_movi_tl(cpu_gpr_d[a->s1_d], a->const4);
    return true;
}

bool trans16_srr_mov(DisasContext *ctx, arg_srr_mov *a, uint16_t insn)
{
    tcg_gen_mov_tl(cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s2]);
    return true;
}

bool trans16_src_mov_a(DisasContext *ctx, arg_src_mov_a *a, uint16_t insn)
{
    //FIXME: Sign extend const4
    tcg_gen_movi_tl(cpu_gpr_a[a->s1_d], a->const4);
    return true;
}

bool trans16_srr_mov_a(DisasContext *ctx, arg_srr_mov_a *a, uint16_t insn)
{
    tcg_gen_mov_tl(cpu_gpr_a[a->s1_d], cpu_gpr_d[a->s2]);
    return true;
}

bool trans16_mov_aa(DisasContext *ctx, arg_mov_aa *a, uint16_t insn)
{
    tcg_gen_mov_tl(cpu_gpr_a[a->s1_d], cpu_gpr_a[a->s2]);
    return true;
}

bool trans16_mov_d(DisasContext *ctx, arg_mov_d *a, uint16_t insn)
{
    tcg_gen_mov_tl(cpu_gpr_d[a->s1_d], cpu_gpr_a[a->s2]);
    return true;
}

bool trans16_mov_e(DisasContext *ctx, arg_mov_e *a, uint16_t insn)
{
    if (!tricore_has_feature(ctx, TRICORE_FEATURE_16)) {
        return false;
    }
    tcg_gen_movi_tl(cpu_gpr_d[a->s1_d], a->const4);
    tcg_gen_sari_tl(cpu_gpr_d[a->s1_d+1], cpu_gpr_d[a->s1_d], 31);
    return true;
}

bool trans16_mul(DisasContext *ctx, arg_mul *a, uint16_t insn)
{
    gen_mul_i32s(cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s2]);
    return false;
}

bool trans16_nop(DisasContext *ctx, arg_nop *a, uint16_t insn)
{
    return true;
}

bool trans16_not(DisasContext *ctx, arg_not *a, uint16_t insn)
{
    return false;
}

bool trans16_sc_or(DisasContext *ctx, arg_sc_or *a, uint16_t insn)
{
    tcg_gen_ori_tl(cpu_gpr_d[15], cpu_gpr_d[15], a->const8);
    return true;
}

bool trans16_srr_or(DisasContext *ctx, arg_srr_or *a, uint16_t insn)
{
    tcg_gen_or_tl(cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s2]);
    return true;
}

bool trans16_ret(DisasContext *ctx, arg_ret *a, uint16_t insn)
{
    gen_compute_branch(ctx, OPC2_16_SR_RET, 0, 0, 0, 0);
    return true;
}

bool trans16_rfe(DisasContext *ctx, arg_rfe *a, uint16_t insn)
{
    gen_helper_rfe(cpu_env);
    tcg_gen_exit_tb(0);
    ctx->bstate = BS_BRANCH;
    return false;
}

bool trans16_rsub(DisasContext *ctx, arg_rsub *a, uint16_t insn)
{
    return false;
}

bool trans16_sat_b(DisasContext *ctx, arg_sat_b *a, uint16_t insn)
{
    return false;
}

bool trans16_sat_bu(DisasContext *ctx, arg_sat_bu *a, uint16_t insn)
{
    return false;
}

bool trans16_sat_h(DisasContext *ctx, arg_sat_h *a, uint16_t insn)
{
    return false;
}

bool trans16_sat_hu(DisasContext *ctx, arg_sat_hu *a, uint16_t insn)
{
    return false;
}

bool trans16_sh(DisasContext *ctx, arg_sh *a, uint16_t insn)
{
    gen_shi(cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s1_d], a->const4);
    return true;
}

bool trans16_sha(DisasContext *ctx, arg_sha *a, uint16_t insn)
{
    gen_shaci(cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s1_d], a->const4);
    return true;
}

bool trans16_sc_st_a(DisasContext *ctx, arg_sc_st_a *a, uint16_t insn)
{
    gen_offset_st(ctx, cpu_gpr_a[15], cpu_gpr_a[10], a->const8 * 4, MO_LESL);
    return true;
}

bool trans16_sro_st_a(DisasContext *ctx, arg_sro_st_a *a, uint16_t insn)
{
    gen_offset_st(ctx, cpu_gpr_a[15], cpu_gpr_a[a->s2], a->off4 * 4, MO_LESL);
    return true;
}

bool trans16_ssr_st_a(DisasContext *ctx, arg_ssr_st_a *a, uint16_t insn)
{
    tcg_gen_qemu_st_tl(cpu_gpr_a[a->s1], cpu_gpr_a[a->s2], ctx->mem_idx, MO_LEUL);
    return true;
}

bool trans16_ssr_st_a_pi(DisasContext *ctx, arg_ssr_st_a_pi *a, uint16_t insn)
{
    tcg_gen_qemu_st_tl(cpu_gpr_a[a->s1], cpu_gpr_a[a->s2], ctx->mem_idx, MO_LEUL);
    tcg_gen_addi_tl(cpu_gpr_a[a->s2], cpu_gpr_a[a->s2], 4);
    return true;
}

bool trans16_ssro_st_a(DisasContext *ctx, arg_ssro_st_a *a, uint16_t insn)
{
    return false;
}

bool trans16_sro_st_b(DisasContext *ctx, arg_sro_st_b *a, uint16_t insn)
{
    gen_offset_st(ctx, cpu_gpr_d[15], cpu_gpr_a[a->s2], a->off4, MO_UB);
    return true;
}

bool trans16_ssr_st_b(DisasContext *ctx, arg_ssr_st_b *a, uint16_t insn)
{
    tcg_gen_qemu_st_tl(cpu_gpr_d[a->s1], cpu_gpr_a[a->s2], ctx->mem_idx, MO_UB);
    return true;
}

bool trans16_ssr_st_b_pi(DisasContext *ctx, arg_ssr_st_b_pi *a, uint16_t insn)
{
    tcg_gen_qemu_st_tl(cpu_gpr_d[a->s1], cpu_gpr_a[a->s2], ctx->mem_idx, MO_UB);
    tcg_gen_addi_tl(cpu_gpr_a[a->s2], cpu_gpr_a[a->s2], 1);
    return true;
}

bool trans16_ssro_st_b(DisasContext *ctx, arg_ssro_st_b *a, uint16_t insn)
{
    return false;
}

bool trans16_sro_st_h(DisasContext *ctx, arg_sro_st_h *a, uint16_t insn)
{
    gen_offset_st(ctx, cpu_gpr_d[15], cpu_gpr_a[a->s2], a->off4 * 2, MO_LESW);
    return true;
}

bool trans16_ssr_st_h(DisasContext *ctx, arg_ssr_st_h *a, uint16_t insn)
{
    tcg_gen_qemu_st_tl(cpu_gpr_d[a->s1], cpu_gpr_a[a->s2], ctx->mem_idx, MO_LEUW);
    return true;
}

bool trans16_ssr_st_h_pi(DisasContext *ctx, arg_ssr_st_h_pi *a, uint16_t insn)
{
    tcg_gen_qemu_st_tl(cpu_gpr_d[a->s1], cpu_gpr_a[a->s2], ctx->mem_idx, MO_LEUW);
    tcg_gen_addi_tl(cpu_gpr_a[a->s2], cpu_gpr_a[a->s2], 2);
    return true;
}

bool trans16_ssro_st_h(DisasContext *ctx, arg_ssro_st_h *a, uint16_t insn)
{
    return false;
}

bool trans16_sc_st_w(DisasContext *ctx, arg_sc_st_w *a, uint16_t insn)
{
    gen_offset_st(ctx, cpu_gpr_d[15], cpu_gpr_a[10], a->const8 * 4, MO_LESL);
    return true;
}

bool trans16_sro_st_w(DisasContext *ctx, arg_sro_st_w *a, uint16_t insn)
{
    gen_offset_st(ctx, cpu_gpr_d[15], cpu_gpr_a[a->s2], a->off4 * 4, MO_LESL);
    return true;
}

bool trans16_ssr_st_w(DisasContext *ctx, arg_ssr_st_w *a, uint16_t insn)
{
    tcg_gen_qemu_st_tl(cpu_gpr_d[a->s1], cpu_gpr_a[a->s2], ctx->mem_idx, MO_LEUL);
    return true;
}

bool trans16_ssr_st_w_pi(DisasContext *ctx, arg_ssr_st_w_pi *a, uint16_t insn)
{
    tcg_gen_qemu_st_tl(cpu_gpr_d[a->s1], cpu_gpr_a[a->s2], ctx->mem_idx, MO_LEUL);
    tcg_gen_addi_tl(cpu_gpr_a[a->s2], cpu_gpr_a[a->s2], 4);
    return true;
}

bool trans16_ssro_st_w(DisasContext *ctx, arg_ssro_st_w *a, uint16_t insn)
{
    return false;
}

bool trans16_srr_sub(DisasContext *ctx, arg_srr_sub *a, uint16_t insn)
{
    gen_sub_d(cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s2]);
    return true;
}

bool trans16_srr_sub_a15(DisasContext *ctx, arg_srr_sub_a15 *a, uint16_t insn)
{
    gen_sub_d(cpu_gpr_d[a->s1_d], cpu_gpr_d[15], cpu_gpr_d[a->s2]);
    return true;
}

bool trans16_srr_sub_15A(DisasContext *ctx, arg_srr_sub_15A *a, uint16_t insn)
{
    gen_sub_d(cpu_gpr_d[15], cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s2]);
    return true;
}

bool trans16_sub_a(DisasContext *ctx, arg_sub_a *a, uint16_t insn)
{
    tcg_gen_subi_tl(cpu_gpr_a[10], cpu_gpr_a[10], a->const8);
    return true;
}

bool trans16_sub_s(DisasContext *ctx, arg_sub_s *a, uint16_t insn)
{
    gen_subs(cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s2]);
    return true;
}

bool trans16_xor(DisasContext *ctx, arg_xor *a, uint16_t insn)
{
    tcg_gen_xor_tl(cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s1_d], cpu_gpr_d[a->s2]);
    return true;
}
