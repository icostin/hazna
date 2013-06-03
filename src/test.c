#include <c41.h>
#include <hazna.h>

/* test *********************************************************************/
uint_t test (c41_io_t * io_p, c41_ma_t * ma_p, c41_smt_t * smt_p)
{
    hzw_t w;
    hzt_t * t = NULL;
    hzm_t * m = NULL;
    char wini = 0;
    int c, rc;

    c41_io_fmt(io_p, "[hazna] test\n- using engine: $s\n", hzlib_name());
    do
    {
        rc = 1;
        c = hzw_init(&w, ma_p, smt_p, io_p, HZLL_DEBUG);
        if (c) break;
        wini = 1;
        
        c = hzt_create(&w, &t);
        if (c) break;

        c = hzm_create(&w, &m);
        if (c) break;

        c = hzm_add_proc(m);
        if (c) break;

        c = hzm_add_iblk(m);
        if (c) break;

        c = hzm_add_insn(m, HZO_DEBUG_OUTPUT_8, 'h', 0, 0);
        if (c) break;
        c = hzm_add_insn(m, HZO_DEBUG_OUTPUT_8, 'e', 0, 0);
        if (c) break;
        c = hzm_add_insn(m, HZO_RET, 0, 0, 0);
        if (c) break;

        rc = 0;
    }
    while (0);

    if (m)
    {
        c = hzm_destroy(m);
        if (c) rc |= 2;
    }

    if (t)
    {
        c = hzt_destroy(t);
        if (c) rc |= 2;
    }

    if (wini)
    {
        c = hzw_finish(&w);
        if (c) rc |= 2;
    }

    c41_io_fmt(io_p, "test result: $Ui\n", rc);
    return rc;
}

