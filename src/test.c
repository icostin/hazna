#include <hza.h>

#define DO(_expr) if ((hze = (_expr))) { rc |= 1; break; } else ((void) 0)

uint8_t test (c41_io_t * log_io, c41_ma_t * ma, c41_smt_t * smt)
{
    uint8_t rc;
    hza_error_t hze;
    hza_context_t hcd;
    char inited = 0;
    hza_module_t * m;
    //uint32_t apx;
    //uint32_t abx;
    uint32_t bbx;

    rc = 0;

    do
    {
        if (c41_io_fmt(log_io, "* using lib: $s\n", hza_lib_name()) < 0)
        {
            rc |= 2;
            break;
        }
        if (c41_io_fmt(log_io, "* ma: $p\n", ma) < 0) { rc |= 2; break; }

        DO(hza_init(&hcd, ma, smt, log_io, HZA_LL_DEBUG));
        inited = 1;

        DO(hza_create_module(&hcd, &m));

        //apx = m->proc_count;
        DO(hza_add_proc(&hcd, m));

        //abx = m->block_count;
        DO(hza_add_block(&hcd, m));

        bbx = m->block_count;
        DO(hza_add_block(&hcd, m));

        DO(hza_add_insn(&hcd, m, HZAO_NOP, 0, 0, 0));
        DO(hza_add_insn(&hcd, m, HZAO_ZXCONST_32, 0x00, 'H', 0));
        DO(hza_add_insn(&hcd, m, HZAO_ZXCONST_32, 0x20, 'e', 0));
        DO(hza_add_insn(&hcd, m, HZAO_ZXCONST_32, 0x40, 'l', 0));
        DO(hza_add_insn(&hcd, m, HZAO_ZXCONST_32, 0x60, 'o', 0));
        DO(hza_add_insn(&hcd, m, HZAO_ZXCONST_32, 0x80, '!', 0));
        DO(hza_add_insn(&hcd, m, HZAO_OUTPUT_DEBUG_CHAR, 0x00, 0, 0));
        DO(hza_add_insn(&hcd, m, HZAO_OUTPUT_DEBUG_CHAR, 0x20, 0, 0));
        DO(hza_add_insn(&hcd, m, HZAO_OUTPUT_DEBUG_CHAR, 0x40, 0, 0));
        DO(hza_add_insn(&hcd, m, HZAO_OUTPUT_DEBUG_CHAR, 0x40, 0, 0));
        DO(hza_add_insn(&hcd, m, HZAO_OUTPUT_DEBUG_CHAR, 0x60, 0, 0));
        DO(hza_add_insn(&hcd, m, HZAO_OUTPUT_DEBUG_CHAR, 0x80, 0, 0));
        DO(hza_add_target(&hcd, m, bbx));

        DO(hza_add_proc(&hcd, m));
        DO(hza_add_proc(&hcd, m));

        DO(hza_release_module(&hcd, m));
    }
    while (0);
    if (inited) hze = hza_finish(&hcd);

    return rc;
}

