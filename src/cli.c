#include <c41.h>
#include <hbs1.h>
#include <hazna.h>

typedef struct bsp_ctx_s                        bsp_ctx_t;
struct bsp_ctx_s
{
    c41_io_t * in;
    c41_io_t * out;
    c41_io_t * log;
};

uint8_t test (c41_io_t * log, c41_ma_t * ma, c41_smt_t * smt);
uint8_t bsp (c41_cli_t * cli_p, uint8_t const * module_path_utf8);

enum cmd_enum
{
    CMD_NONE = 0,
    CMD_VER,
    CMD_BAD,
    CMD_HELP,
    CMD_TEST,
    CMD_BSP, // byte stream processor
};

#define EC_NONE                 0x00
#define EC_PROC                 0x01
#define EC_INIT                 0x02
#define EC_FINISH               0x04
#define EC_INVOKE               0x08
#define EC_LOG                  0x10

/* hmain ********************************************************************/
uint8_t C41_CALL hmain (c41_cli_t * cli_p)
{
    uint_t rc, cmd;
    ssize_t z;

    cmd = CMD_NONE;

    rc = EC_NONE;

    if (!cli_p->arg_n) cmd = CMD_HELP;
    else if (C41_STR_EQUAL(cli_p->arg_a[0], "version")) cmd = CMD_VER;
    else if (C41_STR_EQUAL(cli_p->arg_a[0], "help")) cmd = CMD_HELP;
    else if (C41_STR_EQUAL(cli_p->arg_a[0], "-h")) cmd = CMD_HELP;
    else if (C41_STR_EQUAL(cli_p->arg_a[0], "--help")) cmd = CMD_HELP;
    else if (C41_STR_EQUAL(cli_p->arg_a[0], "test")) cmd = CMD_TEST;
    else if (C41_STR_EQUAL(cli_p->arg_a[0], "bsp")) cmd = CMD_BSP;
    else cmd = CMD_BAD;

    switch (cmd)
    {
    case CMD_TEST:
        rc = test(cli_p->stdout_p, cli_p->ma_p, cli_p->smt_p);
        break;

    case CMD_VER:
        z = c41_io_fmt(cli_p->stdout_p,
                       "hazna-cli_p-v00-"
#if _DEBUG
                       "debug"
#else
                       "release"
#endif
                       " $s\n", hza_lib_name());
        if (z < 0) rc |= EC_LOG;
        break;

    case CMD_BAD:
        rc |= EC_INVOKE;
        z = c41_io_fmt(cli_p->stderr_p,
                       "Error: bad command '$s'\n", cli_p->arg_a[0]);
        if (z < 0) rc |= EC_LOG;

    case CMD_HELP:
        z = c41_io_fmt(cli_p->stdout_p,
 "Usage: hazna CMD [PARAMS]\n"
 "Commands:\n"
 "  version                     prints versions for this tool and the engine\n"
 "  help                        prints this text\n"
 "  test                        runs some tests\n"
 "  bsp MODULE                  byte stream processor\n"
 "Return code is a bitmask of:\n"
 "  1                           processing error\n"
 "  2                           init error\n"
 "  4                           finish error\n"
 "  8                           invokation error (bad args)\n"
 " 16                           logging error\n"
 );
        if (z < 0) rc |= EC_LOG;
        break;

    case CMD_BSP:
        if (cli_p->arg_n != 2)
        {
            rc |= EC_INVOKE;
            z = c41_io_fmt(cli_p->stderr_p, 
                "Error: expecting exactly 1 argument for command 'bsp'\n");
            if (z < 0) rc |= EC_LOG;
            break;
        }
        rc = bsp(cli_p, (uint8_t const *) cli_p->arg_a[1]);

    default:
        break;
    }

    return rc;
}

/* bsp **********************************************************************/
uint8_t bsp (c41_cli_t * cli_p, uint8_t const * module_path_utf8)
{
    bsp_ctx_t ctx;
    ssize_t z;
    uint8_t rc;
    uint_t fsie;
    uint_t mae;
    uint8_t * module_data;
    size_t module_size;
    c41_io_t * in = cli_p->stdin_p;
    c41_io_t * out = cli_p->stdout_p;
    c41_io_t * log = cli_p->stderr_p;

    C41_VAR_ZERO(ctx);

    (void) in;
    (void) out;

    module_data = NULL;
    rc = EC_NONE;
    do
    {
        fsie = c41_file_load_u8p(module_path_utf8, 
                                 C41_STR_LEN(module_path_utf8),
                                 cli_p->fspi_p, cli_p->fsi_p, cli_p->ma_p,
                                 &module_data, &module_size);
        if (fsie)
        {
            rc |= EC_INIT;
            z = c41_io_fmt(log,
                           "Error: failed to load module $s (code $Ui)\n",
                           module_path_utf8, fsie);
            if (z < 0) rc |= EC_LOG;
            break;
        }

    }
    while (0);

    if (module_data)
    {
        mae = c41_ma_free(cli_p->ma_p, module_data, module_size);
        if (mae) rc |= EC_FINISH;
    }

    return rc;
}

