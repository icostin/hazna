#include <c41.h>
#include <hazna.h>

/* test *********************************************************************/
uint_t test (c41_io_t * io_p, c41_ma_t * ma_p, c41_smt_t * smt_p)
{
    hzw_t w;
    hzt_t * t;
    int c, rc;

    c41_io_fmt(io_p, "[hazna] test\n- using engine: $s\n", hzlib_name());
    rc = 0;
    c = hzw_init(&w, ma_p, smt_p, io_p, HZLL_DEBUG);
    if (c) rc = 1;
    else
    {
        c = hzt_create(&w, &t);
        if (c) rc = 4;
        else
        {
            c = hzt_destroy(t);
            if (c) rc |= 8;
        }

        c = hzw_finish(&w);
        if (c) rc |= 2;
    }
    c41_io_fmt(io_p, "test result: $Ui\n", rc);
    return rc;
}

