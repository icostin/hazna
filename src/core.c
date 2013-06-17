#include <stdarg.h>
#include "../include/hza.h"

/* macros *******************************************************************/
#define L(_hc, _level, ...) \
    if ((_hc)->world->log_level >= (_level)) \
        (log_msg((_hc), __FUNCTION__, __FILE__, __LINE__, \
                 (_level), __VA_ARGS__)); \
    else ((void) 0)

#if _DEBUG
#   define D(...) L(hc, HZA_LL_DEBUG, __VA_ARGS__)
#   define ASSERT(_cond) \
    if ((_cond)) ; \
    else do { F("ASSERTION FAILED: $s", #_cond); return HZAF_BUG; } while (0)
#else
#   define D(...)
#   define ASSERT(_cond)
#endif

#define I(...) L(hc, HZA_LL_INFO, __VA_ARGS__)
#define W(...) L(hc, HZA_LL_WARNING, __VA_ARGS__)
#define E(...) L(hc, HZA_LL_ERROR, __VA_ARGS__)
#define F(...) L(hc, HZA_LL_FATAL, __VA_ARGS__)

#define WORLD_SIZE (sizeof(hza_world_t) + smt->mutex_size * 4)

/* static functions *********************************************************/

/* log_msg ******************************************************************/
/**
 * Logs a message with the world's log mutex locked.
 * If there's any error locking/unlocking or writing the message, the function
 * will just disable logging.
 **/
static void log_msg
(
    hza_context_t * hc,
    char const * func,
    char const * src,
    int line,
    int level,
    char const * fmt,
    ...
);

/* init_logging *************************************************************/
/**
 * Initialises the log mutex and world variables for logging.
 **/
static hza_error_t init_logging
(
    hza_context_t * hc,
    c41_io_t * log_io,
    uint8_t log_level
);

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
);

/* alloc_module *************************************************************/
/**
 * Allocates data for a module.
 * This should be called with the world mutex locked.
 * Fills in hc->args[0] with the pointer to the  allocated module.
 * The module is not linked into the world, just allocated.
 */
static hza_error_t C41_CALL alloc_module
(
    hza_context_t * hc
);

/* free_module **************************************************************/
/**
 * Frees data for a module.
 * This should be called with the world mutex locked.
 * The function work with partially initialised modules so that
 * alloc_module() can call this if it encounters errors at some allocation
 * The module to be freed is passed in hc->args[0]
 * Returns:
 *  0                           ok
 *  HZAF_FREE                   likely heap corruption while freeing;
 *                              hc->ma_free_error has the ma error
 **/
static hza_error_t C41_CALL free_module
(
    hza_context_t * hc
);

/* init_module **************************************************************/
/**
 * Initialises a just-created module into the unbound module list and
 * assigns current context as the owner (the only one allowed to change).
 * This should be called while holding the module mutex.
 * The module is passed in hc->args[0]
 * Returns:
 *  0                           ok
 **/
static hza_error_t C41_CALL init_module
(
    hza_context_t * hc
);

/* release_module ***********************************************************/
/**
 * Releases ownership of a module.
 * The module is passed in hc->args[0]
 **/
static hza_error_t C41_CALL release_module
(
    hza_context_t * hc
);

/* realloc_table ************************************************************/
/**
 * Reallocates a given array.
 * Should be called while world mutex is locked!
 * hc->args[0] - is the current array data pointer
 * hc->argc[1] - item size
 * hc->args[2] - desired item count
 * hc->args[3] - current item count
 * The function returns the new pointer in hc->args[0], if successful.
 * Returns:
 *  0                           success
 *  HZAE_ALLOC                  alloc failed
 *  HZAF_ALLOC                  heap corruption
 **/
static hza_error_t C41_CALL realloc_table
(
    hza_context_t * hc
);


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
    I("initing world $#G4p (log level $i)", w, log_level);
    w->init_state |= HZA_INIT_LOG_MUTEX;

    return 0;
}

/* run_locked ***************************************************************/
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
        F("failed locking mutex $#G4p (smt error: $i)", mutex, smte);
        hc->smt_error = smte;
        return (hc->hza_error = HZAF_MUTEX_LOCK);
    }

    e = func(hc);

    smte = c41_smt_mutex_unlock(w->smt, mutex);
    if (smte)
    {
        F("failed unlocking mutex $#G4p (smt error: $i)", mutex, smte);
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
    D("inited context $#G4p and world $#G4p", hc, w);
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
    hza_module_t * m;
    hza_module_t * mn;
    int mae, smte, cc, dirty;
    hza_error_t e;

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
        D("finished context $#G4p, world context count: $Ui", hc, cc);
        if (cc > 0) return hc->hza_finish_error = 0;
    }

    /* destroy the world */
    I("ending world $#G4p...", w);

    for (m = (hza_module_t *) w->unbound_module_list.next;
         m != (hza_module_t *) &w->unbound_module_list;
         m = mn)
    {
        mn = (hza_module_t *) m->links.next;
        hc->args[0] = (intptr_t) m;
        e = free_module(hc);
        if (e)
        {
            F("failed freeing module $Ui: $s = $i", m->module_id,
              hza_error_name(e), e);
            return e;
        }
    }

    if (w->mac.total_size || w->mac.count)
    {
        E("******** MEMORY LEAK: count = $z, size = $z = $Xz ********",
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
static hza_error_t C41_CALL alloc_module
(
    hza_context_t * hc
)
{
    hza_world_t * w = hc->world;
    hza_module_t * m;
    int mae;
    hza_error_t e;

    mae = C41_VAR_ALLOC1Z(&w->mac.ma, m);
    hc->args[0] = (intptr_t) m;
    if (mae)
    {
        E("failed allocating module (ma error $i)\n", mae);
        hc->ma_error = mae;
        return (hc->hza_error = HZAE_ALLOC);
    }

    m->proc_limit = 2;
    mae = C41_VAR_ALLOCZ(&w->mac.ma, m->proc_table, m->proc_limit);
    if (mae)
    {
        E("failed allocating proc table for new module (ma error $i)\n", mae);
        hc->ma_error = mae;
        e = free_module(hc);
        if (e) return e;
        return (hc->hza_error = HZAE_ALLOC);
    }

    m->block_limit = 2;
    mae = C41_VAR_ALLOCZ(&w->mac.ma, m->block_table, m->block_limit);
    if (mae)
    {
        E("failed allocating block table for new module (ma error $i)\n", mae);
        hc->ma_error = mae;
        e = free_module(hc);
        if (e) return e;
        return (hc->hza_error = HZAE_ALLOC);
    }

    m->insn_limit = 2;
    mae = C41_VAR_ALLOCZ(&w->mac.ma, m->insn_table, m->insn_limit);
    if (mae)
    {
        E("failed allocating insn table for new module (ma error $i)\n", mae);
        hc->ma_error = mae;
        e = free_module(hc);
        if (e) return e;
        return (hc->hza_error = HZAE_ALLOC);
    }

    m->target_limit = 2;
    mae = C41_VAR_ALLOCZ(&w->mac.ma, m->target_table, m->target_limit);
    if (mae)
    {
        E("failed allocating target table for new module (ma error $i)\n", mae);
        hc->ma_error = mae;
        e = free_module(hc);
        if (e) return e;
        return (hc->hza_error = HZAE_ALLOC);
    }

    return 0;
}

/* free_module **************************************************************/
static hza_error_t C41_CALL free_module
(
    hza_context_t * hc
)
{
    hza_world_t * w = hc->world;
    hza_module_t * m = (hza_module_t *) hc->args[0];
    int mae;

    if (!m) return 0;

    if (m->proc_table)
    {
        mae = C41_VAR_FREE(&w->mac.ma, m->proc_table, m->proc_limit);
        if (mae)
        {
            F("failed freeing proc table for module (ma error $i)\n", mae);
            hc->ma_free_error = mae;
            return (hc->hza_error = HZAF_FREE);
        }
    }

    if (m->block_table)
    {
        mae = C41_VAR_FREE(&w->mac.ma, m->block_table, m->block_limit);
        if (mae)
        {
            F("failed freeing block table for module (ma error $i)\n", mae);
            hc->ma_free_error = mae;
            return (hc->hza_error = HZAF_FREE);
        }
    }

    if (m->insn_table)
    {
        mae = C41_VAR_FREE(&w->mac.ma, m->insn_table, m->insn_limit);
        if (mae)
        {
            F("failed freeing insn table for module (ma error $i)\n", mae);
            hc->ma_free_error = mae;
            return (hc->hza_error = HZAF_FREE);
        }
    }

    if (m->target_table)
    {
        mae = C41_VAR_FREE(&w->mac.ma, m->target_table, m->target_limit);
        if (mae)
        {
            F("failed freeing target table for module (ma error $i)\n", mae);
            hc->ma_free_error = mae;
            return (hc->hza_error = HZAF_FREE);
        }
    }

    mae = C41_VAR_FREE1(&w->mac.ma, m);
    if (mae)
    {
        F("error freeing module (ma error $i)\n", mae);
        hc->ma_free_error = mae;
        return (hc->hza_error = HZAF_FREE);
    }

    return 0;
}

/* init_module **************************************************************/
static hza_error_t C41_CALL init_module
(
    hza_context_t * hc
)
{
    hza_world_t * w = hc->world;
    hza_module_t * m = (hza_module_t *) hc->args[0];
    m->owner = hc;
    m->module_id = w->module_id_seed++;
    m->block_count = 1;
    m->block_unsealed = 1;
    m->block_unused = 1;
    C41_DLIST_APPEND(w->unbound_module_list, m, links);
    D("inited module: ptr=$#G4p id=$Ui", m, m->module_id);
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
    hza_error_t e, fe;
    hza_module_t * m;

    e = run_locked(hc, alloc_module, w->world_mutex);
    if (e)
    {
        E("failed alocating module in locked state: $s = $i",
          hza_error_name(e), e);
        return e;
    }

    *mp = m = (hza_module_t *) hc->args[0];

    e = run_locked(hc, init_module, w->module_mutex);
    if (e)
    {
        E("failed initialising module (with module mutex locked): $s = $i",
          hza_error_name(e), e);
        fe = run_locked(hc, free_module, w->world_mutex);
        if (fe)
        {
            F("failed freeing module after its init failed: $s = $i",
              hza_error_name(fe), fe);
            return fe;
        }
        return e;
    }

    return (0);
}

/* releaSE_MODULE ***********************************************************/
static hza_error_t C41_CALL release_module
(
    hza_context_t * hc
)
{
    hza_module_t * m = (hza_module_t *) hc->args[0];

    ASSERT(m->owner == hc);
    m->owner = NULL;
    return 0;
}

/* hza_release_module *******************************************************/
HZA_API hza_error_t C41_CALL hza_release_module
(
    hza_context_t * hc,
    hza_module_t * m
)
{
    hza_world_t * w = hc->world;
    hza_error_t e;

    hc->args[0] = (intptr_t) m;
    e = run_locked(hc, release_module, w->module_mutex);

    return e;
}
    
/* realloc_table ************************************************************/
static hza_error_t C41_CALL realloc_table
(
    hza_context_t * hc
)
{
    hza_world_t * w = hc->world;
    int mae;
    mae = c41_ma_realloc_array(&w->mac.ma, (void * *) &hc->args[0],
                               hc->args[1], hc->args[2], hc->args[3]);
    if (mae)
    {
        E("failed reallocating table: ma error $Ui", mae);
        return (hc->hza_error = HZAE_ALLOC);
    }
    return 0;
}


/* hza_add_proc *************************************************************/
HZA_API hza_error_t C41_CALL hza_add_proc
(
    hza_context_t * hc,
    hza_module_t * m
)
{
    hza_world_t * w = hc->world;
    uint32_t i;
    hza_error_t e;

    ASSERT(m->owner == hc);
    if (m->proc_count == m->proc_limit)
    {
        hc->args[0] = (intptr_t) m->proc_table;
        hc->args[1] = sizeof(hza_proc_t);
        hc->args[2] = m->proc_limit << 1;
        hc->args[3] = m->proc_limit;
        e = run_locked(hc, realloc_table, w->module_mutex);
        if (e)
        {
            E("failed reallocating proc table for module m$.4Hd: $s = $Ui",
              m->module_id, hza_error_name(e), e);
            return e;
        }
        m->proc_table = (hza_proc_t *) hc->args[0];
        m->proc_limit <<= 1;
        D("resized proc table for m$.4Hd to $Ui items", 
          m->module_id, m->proc_limit);
    }
    i = m->proc_count++;

    C41_VAR_ZERO(m->proc_table[i]);
    hc->args[0] = i;
    D("added proc: m$.4Hd.p$.4Hd", m->module_id, i);

    return 0;
}

/* hza_add_block ************************************************************/
HZA_API hza_error_t C41_CALL hza_add_block
(
    hza_context_t * hc,
    hza_module_t * m
)
{
    hza_world_t * w = hc->world;
    uint32_t i;
    hza_error_t e;

    ASSERT(m->owner == hc);
    if (m->block_count == m->block_limit)
    {
        hc->args[0] = (intptr_t) m->block_table;
        hc->args[1] = sizeof(hza_block_t);
        hc->args[2] = m->block_limit << 1;
        hc->args[3] = m->block_limit;
        e = run_locked(hc, realloc_table, w->module_mutex);
        if (e)
        {
            E("failed reallocating block table for module m$.4Hd: $s = $Ui",
              m->module_id, hza_error_name(e), e);
            return e;
        }
        m->block_table = (hza_block_t *) hc->args[0];
        m->block_limit <<= 1;
        D("resized block table for m$.4Hd to $Ui items", 
          m->module_id, m->block_limit);
    }
    i = m->block_count++;

    C41_VAR_ZERO(m->block_table[i]);
    hc->args[0] = i;
    D("added block: m$.4Hd.b$.4Hd", m->module_id, i);

    return 0;
}

/* hza_add_target ***********************************************************/
HZA_API hza_error_t C41_CALL hza_add_target
(
    hza_context_t * hc,
    hza_module_t * m,
    uint32_t block_index
)
{
    hza_world_t * w = hc->world;
    uint32_t i;
    hza_error_t e;

    ASSERT(m->owner == hc);
    if (m->target_count == m->target_limit)
    {
        hc->args[0] = (intptr_t) m->target_table;
        hc->args[1] = sizeof(uint32_t);
        hc->args[2] = m->target_limit << 1;
        hc->args[3] = m->target_limit;
        e = run_locked(hc, realloc_table, w->module_mutex);
        if (e)
        {
            E("failed reallocating target table for module m$.4Hd: $s = $Ui",
              m->module_id, hza_error_name(e), e);
            return e;
        }
        m->target_table = (uint32_t *) hc->args[0];
        m->target_limit <<= 1;
        D("resized target table for m$.4Hd to $Ui items", 
          m->module_id, m->target_limit);
    }
    i = m->target_count++;

    m->target_table[i] = block_index;
    hc->args[0] = i;
    D("added target: m$.4Hd.t$.4Hd = b$.04Hd", m->module_id, i, block_index);

    return 0;
}

/* hza_add_insn *************************************************************/
HZA_API hza_error_t C41_CALL hza_add_insn
(
    hza_context_t * hc,
    hza_module_t * m,
    uint16_t opcode,
    uint16_t a,
    uint16_t b,
    uint16_t c
)
{
    hza_world_t * w = hc->world;
    uint32_t i;
    hza_error_t e;

    ASSERT(m->owner == hc);
    if (m->insn_count == m->insn_limit)
    {
        hc->args[0] = (intptr_t) m->insn_table;
        hc->args[1] = sizeof(hza_insn_t);
        hc->args[2] = m->insn_limit << 1;
        hc->args[3] = m->insn_limit;
        e = run_locked(hc, realloc_table, w->module_mutex);
        if (e)
        {
            E("failed reallocating insn table for module m$.4Hd: $s = $Ui",
              m->module_id, hza_error_name(e), e);
            return e;
        }
        m->insn_table = (hza_insn_t *) hc->args[0];
        m->insn_limit <<= 1;
        D("resized insn table for m$.4Hd to $Ui items", 
          m->module_id, m->insn_limit);
    }
    i = m->insn_count++;

    m->insn_table[i].opcode = opcode;
    m->insn_table[i].a = a;
    m->insn_table[i].b = b;
    m->insn_table[i].c = c;

    hc->args[0] = i;
    D("added insn: m$.4Hd.i$.4Hd = $w $w $w $w", 
      m->module_id, i, opcode, a, b, c);

    return 0;
}

