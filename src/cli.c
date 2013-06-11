#include <c41.h>
#include <hbs1.h>
#include <hza.h>

uint8_t test (c41_io_t * log_io, c41_ma_t * ma, c41_smt_t * smt);

enum cmd_enum
{
    CMD_NONE = 0,
    CMD_HELP,
    CMD_TEST
};

/* hmain ********************************************************************/
uint8_t C41_CALL hmain (c41_cli_t * cli_p)
{
    uint_t rc, cmd;

    cmd = CMD_NONE;

    rc = 0;

    if (!cli_p->arg_n) cmd = CMD_HELP;
    else if (C41_STR_EQUAL(cli_p->arg_a[0], "help")) cmd = CMD_HELP;
    else if (C41_STR_EQUAL(cli_p->arg_a[0], "-h")) cmd = CMD_HELP;
    else if (C41_STR_EQUAL(cli_p->arg_a[0], "--help")) cmd = CMD_HELP;
    else if (C41_STR_EQUAL(cli_p->arg_a[0], "test")) cmd = CMD_TEST;

    switch (cmd)
    {
    case CMD_TEST:
        rc = test(cli_p->stdout_p, cli_p->ma_p, cli_p->smt_p);
        break;
    case CMD_HELP:
        c41_io_fmt(cli_p->stdout_p, 
 "Usage: hazna CMD [PARAMS]\n"
 "Commands:\n"
 "  help                        prints this text\n"
 "  test                        runs some tests\n"
 );
        rc = 0;
        break;
    default:
        break;
    }

    return rc;
}

