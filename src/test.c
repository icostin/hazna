#include <hazna.h>

#define DO(_expr) if ((hze = (_expr))) { err_line = __LINE__; rc |= 1; break; } else ((void) 0)

/* test *********************************************************************/
uint8_t test (c41_io_t * log_io, c41_ma_t * ma, c41_smt_t * smt)
{
    uint8_t rc;
    hza_error_t hze;
    hza_context_t hcd;
    char inited = 0;
    int err_line = 0;

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

    }
    while (0);
    if (inited) hze = hza_finish(&hcd);

    if (rc)
    {
        c41_io_fmt(log_io, "error line: $i\n", err_line);
    }

    return rc;
}

