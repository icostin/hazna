#include <hza.h>

uint8_t test (c41_io_t * log_io, c41_ma_t * ma, c41_smt_t * smt)
{
    uint8_t rc;
    hza_error_t hze;
    hza_context_t hcd;
    char inited = 0;
    hza_module_t * m;

    rc = 0;

    do
    {
        if (c41_io_fmt(log_io, "* using lib: $s\n", hza_lib_name()) < 0)
        {
            rc |= 2;
            break;
        }
        if (c41_io_fmt(log_io, "* ma: $p\n", ma) < 0) { rc |= 2; break; }

        hze = hza_init(&hcd, ma, smt, log_io, HZA_LL_DEBUG);
        if (hze) { rc |= 1; break; }
        inited = 1;

        hze = hza_create_module(&hcd, &m);
        if (hze) { rc|= 1; break; }
    }
    while (0);
    if (inited) hze = hza_finish(&hcd);

    return rc;
}

