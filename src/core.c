#include <stdarg.h>
#include "../include/hazna.h"

/* internal configurable constants ******************************************/
#define DEFAULT_STACK_LIMIT 0x10
#define ABSOLUTE_STACK_LIMIT 0x1000
#define INIT_IMPORT_LIMIT 2
#define INIT_REG_SIZE 0x100

/* macros *******************************************************************/
#define L(_hc, _level, ...) \
    if ((_hc)->world->log_level >= (_level)) \
        (log_msg((_hc), __FUNCTION__, __FILE__, __LINE__, \
                 (_level), __VA_ARGS__)); \
    else ((void) 0)

/* DEBUG_CHECK(expr): checks that the given condition is true. the check is
 * performed only on debug builds, it's missing completely on release
 * (unlike assert() where expr is evaluated on release)
 */
#if _DEBUG
#   define D(...) L(hc, HZA_LL_DEBUG, __VA_ARGS__)
#   define DEBUG_CHECK(_cond) \
    if ((_cond)) ; \
    else do { F("ASSERTION FAILED: $s", #_cond); return HZAF_BUG; } while (0)
#else
#   define D(...)
#   define DEBUG_CHECK(_cond)
#endif

#define I(...) L(hc, HZA_LL_INFO, __VA_ARGS__)
#define W(...) L(hc, HZA_LL_WARNING, __VA_ARGS__)
#define E(...) L(hc, HZA_LL_ERROR, __VA_ARGS__)
#define F(...) L(hc, HZA_LL_FATAL, __VA_ARGS__)
#define EF(_e, ...) \
    L(hc, (_e) < HZA_FATAL ? HZA_LL_ERROR : HZA_LL_FATAL, __VA_ARGS__)

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

/* realloc_table_locked *****************************************************/
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
static hza_error_t C41_CALL realloc_table_locked
(
    hza_context_t * hc
);

/* safe_realloc_table *******************************************************/
/**
 * locks world_mutex and reallocates.
 * the new pointer is returned in hc->args.realloc.ptr
 */
static hza_error_t C41_CALL safe_realloc_table
(
    hza_context_t * hc,
    void * old_ptr,
    size_t item_size,
    size_t new_count,
    size_t old_count
);

/* safe_alloc ***************************************************************/
/**
 * Locks world mutex and allocates.
 * the new pointer is returned in hc->args.realloc.ptr
 */
static hza_error_t C41_CALL safe_alloc
(
    hza_context_t * hc,
    size_t size
);

#if 0
/* insn_reg_bits ************************************************************/
/**
 * Computes the minimum number of bits in the register space to allow running
 * the given instruction.
 */
static uint_t insn_reg_bits
(
    hza_insn_t * insn
);

#endif

/* find_mod_name_cell *******************************************************/
/**
 * Builds the tree path in search of the given name.
 * The function should be called with module mutex locked.
 * returns: 0 - name found, 1 - name not found.
 */
static int C41_CALL find_mod_name_cell
(
    hza_context_t *             hc,
    uint8_t const *             name,
    uint_t                      len,
    c41_rbtree_path_t *         path
);

/* alloc_mod_name_cell ******************************************************/
/**
 * Allocate a module name cell.
 * Path has to be initialised previously by find_mod_name_cell().
 * The function should be called with module mutex locked and the lock should
 * not have been released in between find_mod_name_cell() and this call.
 */
static hza_error_t C41_CALL alloc_mod_name_cell
(
    hza_context_t *             hc,
    uint8_t const *             name,
    uint_t                      len,
    c41_rbtree_path_t *         path,
    hza_mod_name_cell_t * *     mnc_p
);

/* get_mod_name_cell ********************************************************/
/**
 * Searches for the given module name and if none is found it allocates a cell.
 * If len < 0 the function will replace it with the C-string length of name.
 * The function should be called with module mutex locked.
 */
static hza_error_t C41_CALL get_mod_name_cell
(
    hza_context_t *             hc,
    void *                      name,
    int                         len,
    hza_mod_name_cell_t * *     mnc_p
);

/* detach_context ***********************************************************/
/**
 * Detaches context from the world.
 * Multithreading state: world_mutex must be locked.
 * Just decrements context_count in the world.
 * fills hc->args.context_count with the number of contexts left attached to
 * the world.
 **/
static hza_error_t C41_CALL detach_context
(
    hza_context_t * hc
);

/* mod_name_cmp *************************************************************/
static uint_t mod_name_cmp
(
    void * key,
    void * node_payload,
    void * context
);

/* destroy_mod_name_cells ***************************************************/
static hza_error_t destroy_mod_name_cells
(
    hza_context_t * hc,
    c41_rbtree_node_t * n
);

/* mod00_load **************************************************************/
/**
 * Loads a module.
 * Should be called with module mutex locked.
 */
static hza_error_t mod00_load
(
    hza_context_t * hc,
    void const * data,
    size_t len
);

#define C32(_v) \
    ((_v) >> 24), ((_v) >> 16) & 0xFF, ((_v) >> 8) & 0xFF, (_v) & 0xFF
#define C16(_v) ((_v) >> 8), ((_v) & 0xFF)

static uint8_t mod00_core[] =
{
    '[', 'h', 'z', 'a', '0', '0', ']', 0x0A,
    C32(0x64),                  // size
    C32(0),                     // name
    C32(0),                     // const128_count
    C32(0),                     // const64_count
    C32(0),                     // const32_count
    C32(0),                     // proc_count
    C32(0),                     // data_block_count
    C32(0),                     // target_block_count
    C32(0),                     // target_count
    C32(1),                     // insn_count
    C32(0),                     // data_size

    /* proc 00 */
    C32(0),                     // insn start
    C32(0),                     // target block start
    C32(0),                     // const32_start
    C32(0),                     // const64_start
    C32(0),                     // const128_start
    C32(0),                     // name
    /* proc 01 */
    C32(1),                     // insn start
    C32(0),                     // target block start
    C32(0),                     // const32_start
    C32(0),                     // const64_start
    C32(0),                     // const128_start
    C32(0),                     // name

    /* insn table */
    C16(HZAO_HALT), C16(0), C16(0), C16(0), // halt
};
#undef C16
#undef C32

/****************************************************************************/
/*                                                                          */
/* Function bodies                                                          */
/*                                                                          */
/****************************************************************************/

/* hza_lib_name *************************************************************/
HAZNA_API char const * C41_CALL hza_lib_name ()
{
    return "hazna-engine-v00"
#if _DEBUG
        "-debug"
#else
        "-release"
#endif
        ;
}

/* hza_error_name ***********************************************************/
HAZNA_API char const * C41_CALL hza_error_name (hza_error_t e)
{
#define X(_x) case _x: return #_x
    switch (e)
    {
        X(HZAE_WORLD_ALLOC);
        X(HZAE_WORLD_FINISH);
        X(HZAE_LOG_MUTEX_INIT);
        X(HZAE_ALLOC);
        X(HZAE_STATE);
        X(HZAE_STACK_LIMIT);
        X(HZAE_PROC_INDEX);
        X(HZAE_MOD00_TRUNC);
        X(HZAE_MOD00_MAGIC);
        X(HZAE_MOD00_CORRUPT);

        X(HZAF_BUG);
        X(HZAF_NO_CODE);
        X(HZAF_MUTEX_LOCK);
        X(HZAF_MUTEX_UNLOCK);
        X(HZAF_WORLD_FREE);
    }

    return e < HZA_FATAL ? "HZAE_UNKNOWN" : "HZAF_UNKNOWN";
#undef X
}

/* hza_opcode_name **********************************************************/
HAZNA_API char const * C41_CALL hza_opcode_name (uint16_t o)
{
#define X(_x) case _x: return #_x
    switch (o)
    {
        X(HZAO_NOP);
        X(HZAO_HALT);
    }
    return "HZAO_UNKNOWN";
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

/* mod_name_cmp *************************************************************/
static uint_t mod_name_cmp
(
    void * key,
    void * node_payload,
    void * context
)
{
    c41_u8an_t * k = key;
    hza_mod_name_cell_t * mnc = node_payload;
    int c;

    (void) context;

    if (k->n != mnc->len)
        return k->n < mnc->len ? C41_RBTREE_SMALLER : C41_RBTREE_GREATER;
    c = C41_MEM_COMPARE(k->a, mnc->name, k->n);
    if (c) return c < 0 ? C41_RBTREE_SMALLER : C41_RBTREE_GREATER;
    return C41_RBTREE_EQUAL;
}

/* hza_init *****************************************************************/
HAZNA_API hza_error_t C41_CALL hza_init
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
    int mae, smte, qi;
    hza_mod_name_cell_t * mnc;

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
    c41_dlist_init(&w->module_list);

    for (qi = 0; qi < HZA_TASK_STATES; qi++)
    {
        c41_dlist_init(&w->task_list[qi]);
    }

    c41_rbtree_init(&w->module_name_tree, mod_name_cmp, NULL);

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

        e = get_mod_name_cell(hc, "core", -1, &mnc);
        if (e)
        {
            E("failed creating module name cell for 'core': $s = $i",
              hza_error_name(e), e);
            break;
        }

        e = mod00_load(hc, mod00_core, sizeof(mod00_core));
        if (e)
        {
            E("failed loading 'core' module: $s = $i", hza_error_name(e), e);
            break;
        }

#if 0
        {
            uint32_t tmp;
            e = get_mod_name_cell(hc, "core", -1, &mnc);
            for (tmp = 0; tmp < 10000000; ++tmp)
            {
                e = get_mod_name_cell(hc, &tmp, 4, &mnc);
                if (e)
                {
                    E("failed initing tmp cell $i", tmp);
                    break;
                }
                mnc->module = (void *) ((intptr_t) tmp + 5);
            }

            for (tmp = 0; tmp < 10000000; ++tmp)
            {
                e = get_mod_name_cell(hc, &tmp, 4, &mnc);
                if (e)
                {
                    E("failed retrieving tmp cell $i", tmp);
                    break;
                }
                if ((intptr_t) mnc->module != ((intptr_t) tmp + 5))
                {
                    e = HZAF_BUG;
                    E("meh tmp=$i, m=$p", tmp, mnc->module);
                }

            }
        }
#endif
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

/* detach_context *******************************************************************/
static hza_error_t detach_context
(
    hza_context_t * hc
)
{
    hza_world_t * w = hc->world;
    hc->args.context_count = (w->context_count -= 1);
    return 0;
}

/* destroy_mod_name_cells ***************************************************/
static hza_error_t destroy_mod_name_cells
(
    hza_context_t * hc,
    c41_rbtree_node_t * n
)
{
    hza_mod_name_cell_t * mnc;
    hza_error_t e;
    uint_t mae;

    mnc = (void *) (n + 1);
    if (n->left)
    {
        e = destroy_mod_name_cells(hc, n->left);
        if (e) return e;
    }

    if (n->right)
    {
        e = destroy_mod_name_cells(hc, n->right);
        if (e) return e;
    }

    mae = c41_ma_free(&hc->world->mac.ma, n,
                      sizeof(c41_rbtree_node_t) + sizeof(hza_mod_name_cell_t)
                      + mnc->len);
    if (mae)
    {
        F("failed freeing mod name cell: $i", mae);
        return hc->hza_error = HZAF_FREE;
    }
    return 0;
}

/* hza_finish ***************************************************************/
HAZNA_API hza_error_t C41_CALL hza_finish
(
    hza_context_t * hc
)
{
    hza_world_t * w = hc->world;
    c41_smt_t * smt = w->smt;
    int mae, smte, dirty;
    hza_error_t e;

    if ((w->init_state & HZA_INIT_WORLD_MUTEX))
    {
        e = run_locked(hc, detach_context, w->world_mutex);
        if (e)
        {
            F("failed detaching context: $s = $i", hza_error_name(e), e);
            return e;
        }
        /* if there are other contexts working with the world then exit */
        D("finished context $#G4p, world context count: $Ui", hc,
          hc->args.context_count);
        if (hc->args.context_count > 0)
            return (hc->hza_finish_error = 0);
    }

    /* destroy the world */
    I("ending world $#G4p...", w);

    if (w->module_name_tree.root)
    {
        e = destroy_mod_name_cells(hc, w->module_name_tree.root);
        if (e)
        {
            F("failed destroying mod name cell tree: $s = $i",
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

/* find_mod_name_cell *******************************************************/
static int C41_CALL find_mod_name_cell
(
    hza_context_t *             hc,
    uint8_t const *             name,
    uint_t                      len,
    c41_rbtree_path_t *         path
)
{
    c41_u8an_t k;
    int c;

    k.a = (uint8_t *) name;
    k.n = len;
    c = c41_rbtree_find(path, &hc->world->module_name_tree, &k);
    return c;
}


/* alloc_mod_name_cell ******************************************************/
static hza_error_t C41_CALL alloc_mod_name_cell
(
    hza_context_t *             hc,
    uint8_t const *             name,
    uint_t                      len,
    c41_rbtree_path_t *         path,
    hza_mod_name_cell_t * *     mnc_p
)
{
    c41_rbtree_node_t * rbtn;
    hza_mod_name_cell_t * mnc;
    hza_error_t e;

    e = safe_alloc(hc, sizeof(c41_rbtree_node_t) + sizeof(hza_mod_name_cell_t)
                        + len);
    if (e)
    {
        EF(e, "failed to allocate a module name cell: $s = $i",
           hza_error_name(e), e);
        return 0;
    }

    rbtn = hc->args.realloc.ptr;
    c41_rbtree_insert(path, rbtn);
    mnc = (hza_mod_name_cell_t *) (rbtn + 1);
    mnc->len = len;
    C41_MEM_COPY(mnc->name, name, len);
    mnc->name[len] = 0;
    mnc->module = NULL;
    *mnc_p = mnc;

    return 0;
}

/* get_mod_name_cell ********************************************************/
static hza_error_t C41_CALL get_mod_name_cell
(
    hza_context_t *             hc,
    void *                      name,
    int                         len,
    hza_mod_name_cell_t * *     mnc_p
)
{
    c41_rbtree_path_t rbpath;
    int c;
    hza_error_t e;

    if (len < 0) len = C41_STR_LEN(name);
    c = find_mod_name_cell(hc, name, len, &rbpath);
    if (!c)
    {
        *mnc_p = c41_rbtree_last_payload(&rbpath);
        return 0;
    }

    if (c)
    {
        e = alloc_mod_name_cell(hc, name, len, &rbpath, mnc_p);
        if (e) return e;
    }

    return 0;
}

/* realloc_table_locked *****************************************************/
static hza_error_t C41_CALL realloc_table_locked
(
    hza_context_t * hc
)
{
    hza_world_t * w = hc->world;
    int mae;
    mae = c41_ma_realloc_array(&w->mac.ma, (void * *) &hc->args.realloc.ptr,
                               hc->args.realloc.item_size,
                               hc->args.realloc.new_count,
                               hc->args.realloc.old_count);
    if (mae)
    {
        E("failed reallocating table: ma error $Ui", mae);
        return (hc->hza_error = HZAE_ALLOC);
    }
    return 0;
}

/* safe_realloc_table *******************************************************/
static hza_error_t C41_CALL safe_realloc_table
(
    hza_context_t * hc,
    void * old_ptr,
    size_t item_size,
    size_t new_count,
    size_t old_count
)
{
    hza_error_t e;
    hc->args.realloc.ptr = old_ptr;
    hc->args.realloc.item_size = item_size;
    hc->args.realloc.new_count = new_count;
    hc->args.realloc.old_count = old_count;
    e = run_locked(hc, realloc_table_locked, hc->world->world_mutex);
    return e;
}

/* safe_alloc ***************************************************************/
static hza_error_t C41_CALL safe_alloc
(
    hza_context_t * hc,
    size_t size
)
{
    return safe_realloc_table(hc, NULL, size, 1, 0);
}

/* mod00_load ***************************************************************/
static hza_error_t mod00_load
(
    hza_context_t * hc,
    void const * data,
    size_t len
)
{
    hza_mod00_hdr_t lhdr;
    hza_module_t * m;
    hza_error_t e;
    uint32_t ofs, n;
    size_t z;

    n = ofs = HZA_MOD00_MAGIC_LEN + sizeof(hza_mod00_hdr_t);
    if (len < n)
    {
        E("not enough data ($Xz)", len);
        return hc->hza_error = HZAE_MOD00_TRUNC;
    }

    if (!C41_MEM_EQUAL(data, HZA_MOD00_MAGIC, HZA_MOD00_MAGIC_LEN))
    {
        E("bad magic!");
        return hc->hza_error = HZAE_MOD00_MAGIC;
    }

    C41_MEM_COPY(&lhdr, C41_PTR_OFS(data, HZA_MOD00_MAGIC_LEN),
                 sizeof(hza_mod00_hdr_t));
    c41_read_u32be_array((uint32_t *) &lhdr,
                         C41_PTR_OFS(data, HZA_MOD00_MAGIC_LEN),
                         sizeof(hza_mod00_hdr_t) / 4);
    if (lhdr.size > len)
    {
        E("not enough data: header size = $Xd, raw size = $Xz", lhdr.size, len);
        return hc->hza_error = HZAE_MOD00_TRUNC;
    }

    len = lhdr.size;
    if (lhdr.const128_count > ((len - n) >> 4))
    {
        E("not enough data");
        return hc->hza_error = HZAE_MOD00_TRUNC;
    }
    n += lhdr.const128_count << 4;

    if (lhdr.const64_count > ((len - n) >> 3))
    {
        E("not enough data");
        return hc->hza_error = HZAE_MOD00_TRUNC;
    }
    n += lhdr.const64_count << 3;

    if (lhdr.const32_count > ((len - n) >> 2))
    {
        E("not enough data");
        return hc->hza_error = HZAE_MOD00_TRUNC;
    }
    n += lhdr.const32_count << 2;

    if (lhdr.proc_count >= ((len - n) / sizeof(hza_mod00_proc_t)))
    {
        E("not enough data");
        return hc->hza_error = HZAE_MOD00_TRUNC;
    }
    n += (lhdr.proc_count + 1) * sizeof(hza_mod00_proc_t);

    if (lhdr.data_block_count >= ((len - n) >> 2))
    {
        E("not enough data");
        return hc->hza_error = HZAE_MOD00_TRUNC;
    }
    n += (lhdr.data_block_count + 1) << 2;

    if (lhdr.target_block_count >= ((len - n) >> 2))
    {
        E("not enough data");
        return hc->hza_error = HZAE_MOD00_TRUNC;
    }
    n += (lhdr.target_block_count + 1) << 2;

    if (lhdr.target_count >= ((len - n) >> 2))
    {
        E("not enough data");
        return hc->hza_error = HZAE_MOD00_TRUNC;
    }
    n += lhdr.target_count << 2;

    if (lhdr.insn_count > ((len - n) >> 4))
    {
        E("not enough data");
        return hc->hza_error = HZAE_MOD00_TRUNC;
    }
    n += lhdr.insn_count << 4;

    if (lhdr.data_size != len - n)
    {
        if (lhdr.data_size > len - n)
        {
            E("not enough data");
            return hc->hza_error = HZAE_MOD00_TRUNC;
        }
        n += lhdr.data_size;
        E("module size mismatch: declared: $Xd, computed: $Xd",
          lhdr.size, n);
        return hc->hza_error = HZAE_MOD00_TRUNC;
    }

    z = n - sizeof(hza_mod00_hdr_t) + sizeof(hza_module_t);
    z += lhdr.proc_count * sizeof(hza_proc_t);
    e = safe_alloc(hc, z);
    if (e)
    {
        E("failed allocating module storage $Xz", z);
        return e;
    }

    m = hc->args.realloc.ptr;
    m->proc_table = (void *) (m + 1);
    m->proc_count = lhdr.proc_count;
    m->const128_table = (void *) (m->proc_table + lhdr.proc_count);
    m->const128_count = lhdr.const128_count;
    m->const64_table = (void *) (m->const128_table + lhdr.const128_count);
    m->const64_count = lhdr.const64_count;
    m->const32_table = (void *) (m->const64_table + lhdr.const64_count);
    m->const32_count = lhdr.const32_count;

    m->data_block_start_table =
        (void *) (m->const32_table + lhdr.const32_count);
    m->data_block_count = lhdr.data_block_count;
    m->target_block_start_table =
        (void *) (m->data_block_start_table + lhdr.data_block_count + 1);
    m->target_block_count = lhdr.target_block_count;

    m->insn_table =
        (void *) (m->target_block_start_table + m->target_block_count);
    m->insn_count = lhdr.insn_count;

    m->data = (void *) (m->insn_table + m->insn_count);
    m->data_size = lhdr.data_size;

    F("no code");
    return hc->hza_error = HZAF_NO_CODE;
}

#if 0
/* insn_reg_bits ************************************************************/
static uint_t insn_reg_bits
(
    hza_insn_t * insn
)
{
    switch (insn->opcode)
    {
    default:
        return 0;
    }
}
#endif
