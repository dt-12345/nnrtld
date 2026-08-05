extern "C" int __nnmusl_init_dso(
    unsigned char *EX_start, unsigned char *EX_end,
    unsigned char *tdata_start, unsigned char *tdata_end,
    unsigned char *tdata_align_abs, unsigned char *tdata_align_rel,
    unsigned char *tbss_start, unsigned char *tbss_end,
    unsigned char *tbss_align_abs, unsigned char *tbss_align_rel,
    unsigned char *got_plt_start, unsigned char *got_plt_end,
    unsigned char *rela_dyn_start, unsigned char *rela_dyn_end,
    unsigned char *rel_dyn_start, unsigned char *rel_dyn_end,
    unsigned char *rela_plt_start, unsigned char *rela_plt_end,
    unsigned char *rel_plt_start, unsigned char *rel_plt_end,
    unsigned char *DYNAMIC
) {
    return 2;
}