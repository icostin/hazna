#include <stdarg.h>
#include "../include/hza.h"

#define L(_hc, _level, ...) \
    (log_msg((_hc), __FUNCTION__, __FILE__, __LINE__, (_level), __VA_ARGS__))

/* hza_lib_name *************************************************************/
HZA_API char const * C41_CALL hza_lib_name ()
{
    return "hazna"
#if _DEBUG
        "-debug"
#else
        "-release"
#endif
        ;
}

/* log_msg *******************************************************************/
#define E(...) do { \
    do { \
        if (c41_io_fmt(w->log_io, "W[$s:$s:$Ui] logging error: ", \
                       __FUNCTION__, __FILE__, __LINE__) < 0) break; \
        if (c41_io_fmt(w->log_io, __VA_ARGS__) < 0) break; \
        c41_io_fmt(w->log_io, "\n"); \
    } while (0); \
    w->log_level = HZA_LL_NONE; \
    return; } while (0)

static void log_msg
(
    hza_context_t * hc,
    char const * func,
    char const * src,
    int line,
    int level,
    char const * fmt,
    ...
)
{
    va_list va;
    hza_world_t * w = hc->world;
    int smte;

    if (w->log_level == HZA_LL_NONE) return;
    smte = c41_smt_mutex_lock(w->smt, w->log_mutex);
    if (smte) E("failed locking log mutex ($i)", smte);

    va_start(va, fmt);
    if (c41_io_fmt(w->log_io, "$c[$s:$s:$Ui] ", "NFEWID"[level],
                   func, src, line) < 0 ||
        c41_io_vfmt(w->log_io, fmt, va) < 0 ||
        c41_io_fmt(w->log_io, "\n") < 0)
    {
        va_end(va);
        E("failed writing log message");
    }
    va_end(va);

    smte = c41_smt_mutex_unlock(w->smt, w->log_mutex);
    if (smte) E("failed unlocking log mutex ($i)", smte);
}
#undef E

/* init_logging *************************************************************/
static hza_error_t init_logging
(
    hza_context_t * hc,
    c41_io_t * log_io,
    uint8_t log_level
)
{
    hza_world_t * w = hc->world;
    uint_t smte;

#if NDEBUG
    if (log_level == HZA_LL_DEBUG) log_level = HZA_LL_INFO;
#endif
    w->log_level = log_level;
    w->log_io = log_io;
    smte = c41_smt_mutex_create(&w->log_mutex, w->smt, w->ma);
    if (smte)
    {
        hc->smt_error = smte;
        return (hc->hza_error = HZA_LOG_MUTEX_CREATE_FAILED);
    }
    L(hc, HZA_LL_INFO, "starting logging (log level $i)", log_level);

    return 0;
}

/* hza_init *****************************************************************/
HZA_API hza_error_t C41_CALL hza_init
(
    hza_context_t * hc,
    hza_world_t * w,
    c41_ma_t * ma,
    c41_smt_t * smt,
    c41_io_t * log_io,
    uint8_t log_level
)
{
    hza_error_t e;

    C41_VAR_ZERO(*hc);
    C41_VAR_ZERO(*w);
    hc->world = w;
    w->ma = ma;
    w->smt = smt;

    if (log_level)
    {
        e = init_logging(hc, log_io, log_level);
        if (e) return e;
    }
    return 0;
}



