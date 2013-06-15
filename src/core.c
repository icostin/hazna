#include <stdarg.h>
#include "../include/hza.h"

#define L(_hc, _level, ...) \
    if ((_hc)->world->log_level >= (_level)) \
        (log_msg((_hc), __FUNCTION__, __FILE__, __LINE__, \
                 (_level), __VA_ARGS__)); \
    else ((void) 0)

#if _DEBUG
#   define D(...) L(hc, HZA_LL_DEBUG, __VA_ARGS__)
#   define A(_cond) \
    if ((_cond)) ; \
    else do { F("ASSERTION FAILED: $s", #_cond); return HZAF_BUG; } while (0)
#else
#   define D(...)
#   define A(_cond)
#endif

#define I(...) L(hc, HZA_LL_INFO, __VA_ARGS__)
#define W(...) L(hc, HZA_LL_WARNING, __VA_ARGS__)
#define E(...) L(hc, HZA_LL_ERROR, __VA_ARGS__)
#define F(...) L(hc, HZA_LL_FATAL, __VA_ARGS__)

#define WORLD_SIZE (sizeof(hza_world_t) + smt->mutex_size * 4)

/* hza_lib_name *************************************************************/
HZA_API char const * C41_CALL hza_lib_name ()
{
    return "hazna-v00"
#if _DEBUG
        "-debug"
#else
        "-release"
#endif
        ;
}

/* hza_error_name ***********************************************************/
HZA_API char const * C41_CALL hza_error_name (hza_error_t e)
{
#define X(_x) case _x: return #_x
    switch (e)
    {
        X(HZAE_WORLD_ALLOC);
        X(HZAE_WORLD_FINISH);
        X(HZAE_LOG_MUTEX_INIT);
        X(HZAE_ALLOC);

        X(HZAF_BUG);
        X(HZAF_NO_CODE);
        X(HZAF_MUTEX_LOCK);
        X(HZAF_MUTEX_UNLOCK);
        X(HZAF_WORLD_FREE);
    }

    return e < HZA_FATAL ? "HZAE_UNKNOWN" : "HZAF_UNKNOWN";
#undef X
}

/* log_msg *******************************************************************/
#define LME(...) do { \
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
    if (smte) LME("failed locking log mutex ($i)", smte);

    va_start(va, fmt);
    if (c41_io_fmt(w->log_io, "$c[$s:$s:$Ui] ", "NFEWID"[level],
                   func, src, line) < 0 ||
        c41_io_vfmt(w->log_io, fmt, va) < 0 ||
        c41_io_fmt(w->log_io, "\n") < 0)
    {
        va_end(va);
        LME("failed writing log message");
    }
    va_end(va);

    smte = c41_smt_mutex_unlock(w->smt, w->log_mutex);
    if (smte) LME("failed unlocking log mutex ($i)", smte);
}
#undef LME

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
    smte = c41_smt_mutex_init(w->smt, w->log_mutex);
    if (smte)
    {
        hc->smt_error = smte;
        return (hc->hza_error = HZAE_LOG_MUTEX_INIT);
    }
    I("initing world $p (log level $i)", w, log_level);
    w->init_state |= HZA_INIT_LOG_MUTEX;

    return 0;
}

/* run_locked ***************************************************************/
/**
 * Executes func() after it locks the given mutex.
 * If func() returns a fatal error it will just forward that error. Otherwise
 * it will unlock the mutex and forward the return value of func().
 * Returns:
 *  0                           func() returned success and locking/unlocking
 *                              worked fine
 *  HZAF_MUTEX_LOCK             mutex lock failed, func() was not executed
 *  HZAF_MUTEX_UNLOCK           mutex unlock failed, after func() was ran
 *  other error                 func() returned that
 **/
static hza_error_t run_locked
(
    hza_context_t * hc,
    hza_error_t (C41_CALL * func) (hza_context_t * hc),
    c41_smt_mutex_t * mutex
)
{
    hza_world_t * w = hc->world;
    int smte;
    hza_error_t e;

    smte = c41_smt_mutex_lock(w->smt, mutex);
    if (smte)
    {
        F("failed locking mutex $p (smt error: $i)", mutex, smte);
        hc->smt_error = smte;
        return (hc->hza_error = HZAF_MUTEX_LOCK);
    }

    e = func(hc);

    smte = c41_smt_mutex_unlock(w->smt, mutex);
    if (smte)
    {
        F("failed unlocking mutex $p (smt error: $i)", mutex, smte);
        hc->smt_error = smte;
        return (hc->hza_error = HZAF_MUTEX_UNLOCK);
    }

    return e;
}

/* hza_init *****************************************************************/
HZA_API hza_error_t C41_CALL hza_init
(
    hza_context_t * hc,
    c41_ma_t * ma,
    c41_smt_t * smt,
    c41_io_t * log_io,
    uint8_t log_level
)
{
    hza_error_t e;
    hza_world_t * w;
    int mae, smte;

    /* clear the context struct */
    C41_VAR_ZERO(*hc);

    /* allocate space for the world and all needed mutexes */
    mae = c41_ma_alloc_zero_fill(ma, (void * *) &w, WORLD_SIZE);
    if (mae)
    {
        hc->ma_error = mae;
        return (hc->hza_error = HZAE_WORLD_ALLOC);
    }
    /* link the world in the context */
    hc->world = w;

    /* init few basic fields in the world */
    w->world_ma = ma;
    w->smt = smt;
    w->world_mutex = (c41_smt_mutex_t *) (w + 1);
    w->log_mutex = C41_PTR_OFS(w->world_mutex, smt->mutex_size);
    w->module_mutex = C41_PTR_OFS(w->log_mutex, smt->mutex_size);
    w->task_mutex = C41_PTR_OFS(w->module_mutex, smt->mutex_size);
    w->context_count = 1;
    c41_dlist_init(&w->loaded_module_list);
    c41_dlist_init(&w->unbound_module_list);
    c41_dlist_init(&w->task_list[HZA_TASK_READY]);
    c41_dlist_init(&w->task_list[HZA_TASK_SUSPENED]);

    /* init allocator; count allocs to detect leaks */
    c41_ma_counter_init(&w->mac, ma,
                        C41_SSIZE_MAX, C41_SSIZE_MAX, C41_SSIZE_MAX);
    do
    {
        /* init logging as early as possible */
        if (log_level)
        {
            e = init_logging(hc, log_io, log_level);
            if (e) break;
        }

        smte = c41_smt_mutex_init(smt, w->world_mutex);
        if (smte)
        {
            E("failed initing world mutex ($i)", smte);
            break;
        }
        w->init_state |= HZA_INIT_WORLD_MUTEX;

        smte = c41_smt_mutex_init(smt, w->module_mutex);
        if (smte)
        {
            E("failed initing module mutex ($i)", smte);
            break;
        }
        w->init_state |= HZA_INIT_MODULE_MUTEX;

        smte = c41_smt_mutex_init(smt, w->task_mutex);
        if (smte)
        {
            E("failed initing task mutex ($i)", smte);
            break;
        }
        w->init_state |= HZA_INIT_TASK_MUTEX;
    }
    while (0);

    if (e)
    {
        /* failed initing the world */
        hc->hza_error = e;
        /* call finish unless something fatal happened */
        if (e < HZA_FATAL) hza_finish(hc);
        return e;
    }
    D("inited context $p and world $p", hc, w);
    D("mutex size: $z", smt->mutex_size);

    return 0;
}

/* hza_finish ***************************************************************/
HZA_API hza_error_t C41_CALL hza_finish
(
    hza_context_t * hc
)
{
    hza_world_t * w = hc->world;
    c41_smt_t * smt = w->smt;
    int mae, smte, cc, dirty;

    if ((w->init_state & HZA_INIT_WORLD_MUTEX))
    {
        /* lock the world */
        smte = c41_smt_mutex_lock(smt, w->world_mutex);
        if (smte)
        {
            E("failed locking world mutex ($i)", smte);
            hc->smt_error = smte;
            hc->hza_finish_error = HZAF_MUTEX_LOCK;
            return HZAF_MUTEX_LOCK;
        }
        /* decrement the number of contexts pointing to this world */
        cc = (w->context_count -= 1);
        /* unlock the world */
        smte = c41_smt_mutex_unlock(smt, w->world_mutex);
        if (smte)
        {
            E("failed unlocking world mutex ($i)", smte);
            hc->smt_error = smte;
            hc->hza_finish_error = HZAF_MUTEX_UNLOCK;
            return HZAF_MUTEX_UNLOCK;
        }
        /* if there are other contexts working with the world then exit */
        D("finished context $p, world context count: $Ui", hc, cc);
        if (cc > 0) return hc->hza_finish_error = 0;
    }

    /* destroy the world */
    I("ending world $p...", w);

    if (w->mac.total_size || w->mac.count)
    {
        E("memory leak: count = $z, size = $z = $Xz",
          w->mac.count, w->mac.total_size, w->mac.total_size);
    }

    if ((w->init_state & HZA_INIT_WORLD_MUTEX))
    {
        smte = c41_smt_mutex_finish(smt, w->world_mutex);
        if (smte)
        {
            E("failed finishing world mutex ($i)", smte);
            hc->smt_error = smte;
            dirty = 1;
        }
    }

    if ((w->init_state & HZA_INIT_MODULE_MUTEX))
    {
        smte = c41_smt_mutex_finish(smt, w->world_mutex);
        if (smte)
        {
            E("failed finishing world mutex ($i)", smte);
            hc->smt_error = smte;
            dirty = 1;
        }
    }

    if ((w->init_state & HZA_INIT_TASK_MUTEX))
    {
        smte = c41_smt_mutex_finish(smt, w->world_mutex);
        if (smte)
        {
            E("failed finishing world mutex ($i)", smte);
            hc->smt_error = smte;
            dirty = 1;
        }
    }

    if ((w->init_state & HZA_INIT_LOG_MUTEX))
    {
        smte = c41_smt_mutex_finish(smt, w->log_mutex);
        if (smte)
        {
            E("failed finishing logging mutex ($i)", smte);
            hc->smt_error = smte;
            dirty = 1;
        }
    }

    mae = c41_ma_free(w->world_ma, w, WORLD_SIZE);
    if (mae)
    {
        hc->ma_error = mae;
        return HZAF_WORLD_FREE;
    }

    return (hc->hza_finish_error = dirty ? HZAE_WORLD_FINISH : 0);
}

/* alloc_module *************************************************************/
/**
 * This should be called with the world mutex locked.
 * Fills in hc->args[0] with the pointer to the  allocated module.
 * The module is not linked into the world, just allocated.
 */
static hza_error_t C41_CALL alloc_module
(
    hza_context_t * hc
)
{
    hza_world_t * w = hc->world;
    hza_module_t * m;
    int mae;

    mae = c41_ma_alloc_zero_fill(&w->mac.ma, (void * *) &m,
                                 sizeof(hza_module_t));
    if (mae)
    {
        E("failed allocating module (ma error $i)\n", mae);
        hc->ma_error = mae;
        return (hc->hza_error = HZAE_ALLOC);
    }
    hc->args[0] = (intptr_t) m;

    return 0;
}

/* hza_create_module ********************************************************/
HZA_API hza_error_t C41_CALL hza_create_module
(
    hza_context_t * hc,
    hza_module_t * * mp
)
{
    hza_world_t * w = hc->world;
    hza_error_t e;
    hza_module_t * m;

    e = run_locked(hc, alloc_module, w->world_mutex);
    if (e)
    {
        E("failed alocating module in locked state: $s = $i",
          hza_error_name(e), e);
        return e;
    }

    *mp = m = (hza_module_t *) hc->args[0];

    return (hc->hza_error = HZAF_NO_CODE);
}


