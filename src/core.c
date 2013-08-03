#include <stdarg.h>
#include "../include/hazna.h"

/* internal configurable constants ******************************************/
#define INIT_IMPORT_LIMIT       2
#define INIT_REG_SIZE           0x100
#define INIT_FRAME_LIMIT        0x10
#define INIT_MODMAP_LIMIT       8

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

#define WORLD_SIZE \
    (sizeof(hza_world_t) + smt->mutex_size * 4)

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

/* safe_free ****************************************************************/
static hza_error_t C41_CALL safe_free
(
    hza_context_t * hc,
    void * ptr,
    size_t size
);

/* insn_check ***************************************************************/
/**
 * Computes the minimum number of bits in the register space to allow running
 * the given instruction.
 */
static int32_t insn_check
(
    hza_context_t * hc,
    hza_proc_t * proc,
    hza_insn_t * insn
);

/* last_insn_check **********************************************************/
/**
 * Checks if the given instruction is valid for the last instruction in the
 * proc. This means that the flow will not continue to the instruction after it.
 */
static int last_insn_check
(
    hza_insn_t * insn
);
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
 * The allocated module is returned in hc->args.realloc.ptr
 **/
static hza_error_t mod00_load
(
    hza_context_t * hc,
    void const * data,
    size_t len
);

/* task_alloc ***************************************************************/
/**
 *  Allocates a task. This should be called with world mutex locked.
 *  The pointer to the new task is stored in hc->args.task.
 */
static hza_error_t C41_CALL task_alloc
(
    hza_context_t * hc
);

/* task_free ****************************************************************/
/**
 *  Frees task memory. This should be called with world mutex locked.
 *  The task is passed in hc->args.task.
 *  This is intended to be called from task_alloc() (with a partially allocated
 *  task), from hza_task_create() when task_init() fails and from
 *  hza_task_deref() when there are no refs left to the task.
 */
static hza_error_t C41_CALL task_free
(
    hza_context_t * hc
);

/* task_init ****************************************************************/
/**
 *  Inits a newly allocated task. This should be called with task mutex locked.
 */
static hza_error_t C41_CALL task_init
(
    hza_context_t * hc
);
/* mod00_core ***************************************************************/
#define C32(_v) \
    ((_v) >> 24), ((_v) >> 16) & 0xFF, ((_v) >> 8) & 0xFF, (_v) & 0xFF
#define C16(_v) ((_v) >> 8), ((_v) & 0xFF)
static uint8_t mod00_core[] =
{
    /* 0x0000: */ '[', 'h', 'z', 'a', '0', '0', ']', 0x0A,
    /* 0x0008: */ C32(0x130),                  // size (in bytes)
    /* 0x000C: */ C32(0x11223344),            // checksum
    /* 0x0010: */ C32(0),                     // name
    /* 0x0014: */ C32(0),                     // const128_count
    /* 0x0018: */ C32(0),                     // const64_count
    /* 0x001C: */ C32(0),                     // const32_count
    /* 0x0020: */ C32(2),                     // proc_count
    /* 0x0024: */ C32(2),                     // data_block_count
    /* 0x0028: */ C32(2),                     // target_count
    /* 0x002C: */ C32(0x12),                  // insn_count
    /* 0x0030: */ C32(4),                     // data_size
    /* 0x0034: */ C32(0),                     // reserved0
    /* 0x0038: */ C32(0),                     // reserved1
    /* 0x003C: */ C32(0),                     // reserved2

    /* proc 00 */
    /* 0x0040: */ C32(0),                     // insn start
    /* 0x0044: */ C32(0),                     // target start
    /* 0x0048: */ C32(0),                     // const128_start
    /* 0x004C: */ C32(0),                     // const64_start
    /* 0x0050: */ C32(0),                     // const32_start
    /* 0x0054: */ C32(0),                     // name

    /* proc 01 */
    /* 0x0058: */ C32(1),                     // insn start
    /* 0x005C: */ C32(0),                     // target start
    /* 0x0060: */ C32(0),                     // const128_start
    /* 0x0064: */ C32(0),                     // const64_start
    /* 0x0068: */ C32(0),                     // const32_start
    /* 0x006C: */ C32(0),                     // name

    /* proc 02 - END */
    /* 0x0070: */ C32(0x12),                  // insn start
    /* 0x0074: */ C32(2),                     // target start
    /* 0x0078: */ C32(0),                     // const128_start
    /* 0x007C: */ C32(0),                     // const64_start
    /* 0x0080: */ C32(0),                     // const32_start
    /* 0x0084: */ C32(0),                     // name

    /* 0x0088: */ C32(0),                     // data block offset #0
    /* 0x008C: */ C32(0),                     // data block offset #1
    /* 0x0090: */ C32(4),                     // data block offset #2 - END

    /* target table - empty */
    /* 0x0094: */ C32(0x10),
    /* 0x0098: */ C32(0x01),

    /* insn table */
    /* 0x009C: */ C16(HZAO_HALT), C16(0), C16(0), C16(0), // halt
    /* 0x00A4: */ C16(HZAO_INIT_8),  C16(0x90), C16(5),   C16(0),
    /* 0x00AC: */ C16(HZAO_INIT_16), C16(0x00), C16('h'), C16(0),
    /* 0x00B4: */ C16(HZAO_INIT_16), C16(0x10), C16('e'), C16(0),
    /* 0x00BC: */ C16(HZAO_INIT_16), C16(0x20), C16('l'), C16(0),
    /* 0x00C4: */ C16(HZAO_INIT_16), C16(0x30), C16('o'), C16(0),
    /* 0x00CC: */ C16(HZAO_INIT_16), C16(0x70), C16('!'), C16(0),
    /* 0x00D4: */ C16(HZAO_INIT_16), C16(0x80), C16(10),  C16(0),
    /* 0x00DC: */ C16(HZAO_DEBUG_OUT_16), C16(0x00), C16(0), C16(0),
    /* 0x00E4: */ C16(HZAO_DEBUG_OUT_16), C16(0x10), C16(0), C16(0),
    /* 0x00EC: */ C16(HZAO_DEBUG_OUT_16), C16(0x20), C16(0), C16(0),
    /* 0x00F4: */ C16(HZAO_DEBUG_OUT_16), C16(0x20), C16(0), C16(0),
    /* 0x00FC: */ C16(HZAO_DEBUG_OUT_16), C16(0x30), C16(0), C16(0),
    /* 0x0104: */ C16(HZAO_DEBUG_OUT_16), C16(0x70), C16(0), C16(0),
    /* 0x010C: */ C16(HZAO_DEBUG_OUT_16), C16(0x80), C16(0), C16(0),
    /* 0x0114: */ C16(HZAO_WRAP_ADD_CONST_8), C16(0x90), C16(0x90), C16(0xFF),
    /* 0x011C: */ C16(HZAO_BRANCH_ZERO_8), C16(0x90), C16(0), C16(0),
    /* 0x0124: */ C16(HZAO_RET), C16(0), C16(0), C16(0),

    /* data block #1 */
    /* 0x012C: */ 'c', 'o', 'r', 'e',

    /* 0x0130: end */
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
        X(HZAO_RET);
        X(HZAO_DEBUG_OUT_16);
        X(HZAO_DEBUG_OUT_32);
        X(HZAO_INIT_8);
        X(HZAO_INIT_16);
        X(HZAO_WRAP_ADD_CONST_8);
        X(HZAO_BRANCH_ZERO_1);
        X(HZAO_BRANCH_ZERO_2);
        X(HZAO_BRANCH_ZERO_4);
        X(HZAO_BRANCH_ZERO_8);
        X(HZAO_BRANCH_ZERO_16);
        X(HZAO_BRANCH_ZERO_32);
        X(HZAO_BRANCH_ZERO_64);
        X(HZAO_BRANCH_ZERO_128);
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
    if (c41_io_fmt(w->log_io, "$c: ", "NFEWID"[level]) < 0
        || c41_io_vfmt(w->log_io, fmt, va) < 0
        || c41_io_fmt(w->log_io, "    [$s:$s:$Ui]\n", func, src, line) < 0)
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
static uint_t C41_CALL mod_name_cmp
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
        D("initing world $#G4p (log level $i)", w, w->log_level);

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

        w->core_module = hc->args.realloc.ptr;
        mnc->module = hc->args.realloc.ptr;

        smte = c41_smt_cond_create(&hc->cond, smt, &w->mac.ma);
        if (smte)
        {
            e = HZAE_COND_CREATE;
            E("failed initing context condition variable ($i)", smte);
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

/* detach_context ***********************************************************/
static hza_error_t detach_context
(
    hza_context_t * hc
)
{
    hza_world_t * w = hc->world;
    int smte;

    hc->args.context_count = (w->context_count -= 1);
    if (hc->cond)
    {
        smte = c41_smt_cond_destroy(hc->cond, w->smt, &w->mac.ma);
        if (smte)
        {
            F("failed destroying context condition variable ($i)", smte);
            hc->smt_error = smte;
            return hc->hza_error = HZAF_COND_DESTROY;
        }
    }

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
    hza_module_t * m;
    c41_np_t * np;
    int mae, smte, dirty, ts;
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
    D("ending world $#G4p...", w);

    /* destroy tasks */
    for (ts = 0; ts < HZA_TASK_STATES; ++ts)
    {
        for (np = w->task_list[ts].next; np != &w->task_list[ts];)
        {
            hza_task_t * t;
            t = (void *) np;
            np = np->next;
            hc->args.task = t;
            e = task_free(hc);
            if (e)
            {
                F("failed destroying task $i: $s = $i", t->task_id,
                  hza_error_name(e), e);
                return e;
            }
        }
    }

    /* destroy module name tree */
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

    /* destroy modules */
    for (np = w->module_list.next; np != &w->module_list;)
    {
        m = (void *) np;
        np = np->next;
        e = safe_free(hc, m, m->size);
        if (e)
        {
            F("failed freeing module at $Xp: $s = $i", m, hza_error_name(e), e);
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
    D("realloc in: ptr=$p, item_size=$z, new_count=$z, old_count=$z",
      hc->args.realloc.ptr,
      hc->args.realloc.item_size,
      hc->args.realloc.new_count,
      hc->args.realloc.old_count);

    mae = c41_ma_realloc_array(&w->mac.ma, (void * *) &hc->args.realloc.ptr,
                               hc->args.realloc.item_size,
                               hc->args.realloc.new_count,
                               hc->args.realloc.old_count);
    D("realloc out: mae=$Ui, ptr=$p", mae, hc->args.realloc.ptr);
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
    return safe_realloc_table(hc, NULL, 1, size, 0);
}

/* safe_free ****************************************************************/
static hza_error_t C41_CALL safe_free
(
    hza_context_t * hc,
    void * ptr,
    size_t size
)
{
    return safe_realloc_table(hc, ptr, 1, 0, size);
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
    hza_mod00_proc_t * pt;
    hza_module_t * m;
    hza_world_t * w = hc->world;
    uint8_t * p;
    hza_error_t e;
    uint32_t n, i, j;
    size_t z;

    D("len = $Xz", len);
    n = sizeof(hza_mod00_hdr_t);
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

    c41_read_u32be_array((uint32_t *) &lhdr.size,
                         C41_PTR_OFS(data, HZA_MOD00_MAGIC_LEN),
                         (sizeof(hza_mod00_hdr_t) - HZA_MOD00_MAGIC_LEN) / 4);
    D("size:                    $Xd", lhdr.size);
    D("checksum:                $Xd", lhdr.checksum);
    D("name:                    $Xd", lhdr.name);
    D("const128 count:          $Xd", lhdr.const128_count);
    D("const64 count:           $Xd", lhdr.const64_count);
    D("const32 count:           $Xd", lhdr.const32_count);
    D("proc count:              $Xd", lhdr.proc_count);
    D("data block count:        $Xd", lhdr.data_block_count);
    D("target count:            $Xd", lhdr.target_count);
    D("insn count:              $Xd", lhdr.insn_count);
    D("data size:               $Xd", lhdr.data_size);

    if (lhdr.size > len)
    {
        E("not enough data: header size = $Xd, raw size = $Xz", lhdr.size, len);
        return hc->hza_error = HZAE_MOD00_TRUNC;
    }
    len = lhdr.size;

#define CHECK(_cond) \
    if ((_cond)) ; else { E("not enough data"); \
        return hc->hza_error = HZAE_MOD00_TRUNC; }

    D("const128 offset:         $Xd", n);
    CHECK(lhdr.const128_count <= ((len - n) >> 4));
    n += lhdr.const128_count << 4;

    D("const64 offset:          $Xd", n);
    CHECK(lhdr.const64_count <= ((len - n) >> 3));
    n += lhdr.const64_count << 3;

    D("const32 offset:          $Xd", n);
    CHECK(lhdr.const32_count <= ((len - n) >> 2));
    n += lhdr.const32_count << 2;

    D("proc offset:             $Xd", n);
    CHECK(lhdr.proc_count < ((len - n) / sizeof(hza_mod00_proc_t)));
    n += (lhdr.proc_count + 1) * sizeof(hza_mod00_proc_t);

    D("data block offset:       $Xd", n);
    CHECK(lhdr.data_block_count < ((len - n) >> 2));
    n += (lhdr.data_block_count + 1) << 2;

    D("target offset:           $Xd", n);
    CHECK(lhdr.target_count < ((len - n) >> 2));
    n += lhdr.target_count << 2;

    D("insn offset:             $Xd", n);
    CHECK(lhdr.insn_count <= ((len - n) >> 3));
    n += lhdr.insn_count << 3;

    D("data offset:             $Xd", n);
    if (lhdr.data_size != len - n)
    {
        CHECK(lhdr.data_size <= len - n);
        n += lhdr.data_size;
        E("module size mismatch: declared: $Xd, computed: $Xd",
          lhdr.size, n);
        return hc->hza_error = HZAE_MOD00_TRUNC;
    }
    n += lhdr.data_size;

    D("module data:             $Xd / $Xz", n, len);
#undef CHECK

    /* allocate module */
    z = n - sizeof(hza_mod00_hdr_t) + sizeof(hza_module_t)
        + lhdr.proc_count * sizeof(hza_proc_t);
    D("allocating $Xz for module", z);
    e = safe_alloc(hc, z);
    if (e)
    {
        E("failed allocating module storage $Xz", z);
        return e;
    }
    m = hc->args.realloc.ptr;
    m->size = z;

    /* set table pointers */
    m->proc_table = (void *) (m + 1);
    m->proc_count = lhdr.proc_count;

    m->const128_table = (void *) (m->proc_table + lhdr.proc_count);
    m->const128_count = lhdr.const128_count;
    m->const64_table = (void *) (m->const128_table + lhdr.const128_count);
    m->const64_count = lhdr.const64_count;
    m->const32_table = (void *) (m->const64_table + lhdr.const64_count);
    m->const32_count = lhdr.const32_count;

    pt = (void *) (m->const32_table + lhdr.const32_count);

    m->data_block_start_table = (void *) (pt + lhdr.proc_count + 1);
    m->data_block_count = lhdr.data_block_count;

    m->target_table =
        (void *) (m->data_block_start_table + lhdr.data_block_count + 1);

    m->insn_table = (void *) (m->target_table + m->target_count);
    m->insn_count = lhdr.insn_count;

    m->data = (void *) (m->insn_table + m->insn_count);
    m->data_size = lhdr.data_size;
    D("m=$p, end=$p, end-m=$z, z=$z", m, m->data + m->data_size,
      (size_t) C41_PTR_DIFF(m->data + m->data_size, m), z);

    p = C41_PTR_OFS(data, sizeof(hza_mod00_hdr_t));

    /* deserialising 128-bit constants */
    for (i = 0; i < lhdr.const128_count; ++i, p += 0x10)
    {
        m->const128_table[i].high = c41_read_u64be(p);
        m->const128_table[i].low = c41_read_u64be(p + 8);
    }

    /* deserialising 64-bit constants */
    for (i = 0; i < lhdr.const64_count; ++i, p += 8)
        m->const64_table[i] = c41_read_u64be(p);

    /* compute how many 32-bit ints are to be deserialised */
    n = lhdr.const32_count
        + (lhdr.proc_count + 1) * (sizeof(hza_mod00_proc_t) / 4)
        + lhdr.data_block_count + 1
        + lhdr.target_count;

    /* deserialising 32-bit ints */
    c41_read_u32be_array(m->const32_table, p, n);
    p += n * 4;

    /* deserialising 16-bit ints (instructions) */
    c41_read_u16be_array((uint16_t *) m->insn_table, p, lhdr.insn_count * 4);
    p += lhdr.insn_count * 8;

    /* copy 8-bit data */
    C41_MEM_COPY(m->data, p, lhdr.data_size);

#define CHECK(_cond) \
    if ((_cond)) ; else { E("corrupt data"); goto l_corrupted; }

    CHECK(pt[0].insn_start == 0);
    CHECK(pt[0].target_start == 0);
    CHECK(pt[0].const128_start == 0);
    CHECK(pt[0].const64_start == 0);
    CHECK(pt[0].const32_start == 0);

    /* check proc start indexes */
    for (i = 0; i < lhdr.proc_count; ++i)
    {
        CHECK(pt[i].insn_start < pt[i + 1].insn_start);
        CHECK(pt[i].target_start <= pt[i + 1].target_start);
        CHECK(pt[i].const128_start <= pt[i + 1].const128_start);
        CHECK(pt[i].const64_start <= pt[i + 1].const64_start);
        CHECK(pt[i].const32_start <= pt[i + 1].const32_start);
        CHECK(pt[i].name < lhdr.data_block_count);
    }
    CHECK(pt[i].insn_start == lhdr.insn_count);
    CHECK(pt[i].target_start == lhdr.target_count);
    CHECK(pt[i].const128_start == lhdr.const128_count);
    CHECK(pt[i].const64_start == lhdr.const64_count);
    CHECK(pt[i].const32_start == lhdr.const32_count);
    CHECK(pt[i].name == 0);

    /* check data block start indexes */
    CHECK(lhdr.data_block_count >= 1);
    CHECK(m->data_block_start_table[0] == 0);
    CHECK(m->data_block_start_table[1] == 0);
    for (i = 1; i < m->data_block_count; ++i)
    {
        CHECK(m->data_block_start_table[i]
              < m->data_block_start_table[i + 1]);
    }
    CHECK(m->data_block_start_table[i] == lhdr.data_size);

    /* todo: check proc targets & insns */
    for (i = 0; i < lhdr.proc_count; ++i)
    {
        uint32_t rlen = 0;
        hza_proc_t * proc = m->proc_table + i;

        proc->insn_table = m->insn_table + pt[i].insn_start;
        proc->insn_count = pt[i + 1].insn_start - pt[i].insn_start;

        proc->const128_table = m->const128_table + pt[i].const128_start;
        proc->const128_count = pt[i + 1].const128_start - pt[i].const128_start;

        proc->const64_table = m->const64_table + pt[i].const64_start;
        proc->const64_count = pt[i + 1].const64_start - pt[i].const64_start;

        proc->const32_table = m->const32_table + pt[i].const32_start;
        proc->const32_count = pt[i + 1].const32_start - pt[i].const32_start;

        proc->target_table = m->target_table + pt[i].target_start;
        proc->target_count = pt[i + 1].target_start - pt[i].target_start;

        /* validate all targets from all target blocks that belong to
         * current proc */
        for (j = 0; j < proc->target_count; ++j)
        {
            CHECK(proc->target_table[j] < proc->insn_count);
        }

        /* validate all instructions from current proc */
        for (j = 0; j < proc->insn_count; ++j)
        {
            int32_t rl;
            rl = insn_check(hc, proc, proc->insn_table + j);
            D("check P$.4Hd.I$.4Hd: $s ($XUw) $XUw $XUw $XUw => reg_size = $.1Xd",
              i, j, hza_opcode_name(proc->insn_table[j].opcode),
              proc->insn_table[j].opcode,
              proc->insn_table[j].a,
              proc->insn_table[j].b,
              proc->insn_table[j].c,
              rl);
            if (rl < 0)
            {
                E("invalid insn $Xd: $s ($XUw) $XUw $XUw $XUw",
                  j, hza_opcode_name(proc->insn_table[j].opcode),
                  proc->insn_table[j].opcode,
                  proc->insn_table[j].a,
                  proc->insn_table[j].b,
                  proc->insn_table[j].c);
                goto l_corrupted;
            }
            if (rlen < (uint32_t) rl) rlen = rl;
        }
        j--;
        if (last_insn_check(proc->insn_table + j))
        {
            E("invalid last insn $Xd: $s ($XUw) $XUw $XUw $XUw",
              j, hza_opcode_name(proc->insn_table[j].opcode),
              proc->insn_table[j].opcode,
              proc->insn_table[j].a,
              proc->insn_table[j].b,
              proc->insn_table[j].c);
            goto l_corrupted;
        }

        proc->reg_size = rlen >> 3;
        D("proc $.3Xd reg_size:    $.5Xd bytes", i, proc->reg_size);
    }
#undef CHECK

    /* valid module. init remaining fields. */
    C41_DLIST_APPEND(w->module_list, m, links);
    m->module_id = w->module_id_seed++;
    m->task_count = 0;
    m->ctx_count = 1;
    m->size = z;

    return 0;

l_corrupted:
    e = safe_free(hc, m, z);
    if (e)
    {
        F("failed freeing module (load failed due to corrupt data): $s = $Ui",
          hza_error_name(e), e);
        return e;
    }
    return hc->hza_error = HZAE_MOD00_CORRUPT;
}

/* insn_check ***************************************************************/
static int32_t insn_check
(
    hza_context_t * hc,
    hza_proc_t * proc,
    hza_insn_t * insn
)
{
    uint32_t a, b, c, ps, rs;
    uint_t oc;

    oc = HZA_OPCODE_CLASS(insn->opcode);

    /* check operand a */
    switch (oc)
    {
    case HZAOC_NNN:
        rs = 0;
        break;

    case HZAOC_RNN:
    case HZAOC_RRN:
    case HZAOC_RRR:
    case HZAOC_RRC:
    case HZAOC_RRS:
    case HZAOC_RR4:
    case HZAOC_RCN:
    case HZAOC_RNP:
    case HZAOC_RRP:
    case HZAOC_RCP:
    case HZAOC_RRG:
    case HZAOC_RCG:
    case HZAOC_RLT:
    case HZAOC_RAN:
    case HZAOC_RAA:
    case HZAOC_RA4:
    case HZAOC_RA5:
    case HZAOC_RA6:
        ps = 1 << HZA_OPCODE_PRI_SIZE(insn->opcode);
    l_check_a_reg:
        a = insn->a;
        if ((a & (ps - 1)) != 0)
        {
            E("I$.4Hd: unaligned reg (a = $XUw)",
              insn - proc->insn_table, insn->a);
            return -1; // unaligned reg
        }
        rs = a + ps;
        break;

    case HZAOC_QRR:
    case HZAOC_QRC:
    case HZAOC_QRS:
    case HZAOC_QR4:
        ps = 2 << HZA_OPCODE_PRI_SIZE(insn->opcode);
        goto l_check_a_reg;

    case HZAOC_SRN:
        ps = 1 << HZA_OPCODE_SEC_SIZE(insn->opcode);
        goto l_check_a_reg;

    default:
        return -1;
    }

    /* check operand b */
    switch (oc)
    {
    case HZAOC_NNN:
    case HZAOC_RNN:
    case HZAOC_RNP:
    case HZAOC_RLT: // this will be checked at arg c
        break;

    case HZAOC_RRN:
    case HZAOC_RRR:
    case HZAOC_RRC:
    case HZAOC_RRS:
    case HZAOC_RR4:
    case HZAOC_RRP:
    case HZAOC_RRG:
    case HZAOC_QRR:
    case HZAOC_QRC:
    case HZAOC_QRS:
    case HZAOC_QR4:
    case HZAOC_SRN:
        ps = 1 << HZA_OPCODE_PRI_SIZE(insn->opcode);
    l_check_b_reg:
        b = insn->b;
        if ((b & (ps - 1)) != 0)
        {
            E("I$.4Hd: unaligned reg (b = $XUw)",
              insn - proc->insn_table, insn->b);
            return -1; // unaligned reg
        }
        ps += b;
        if (rs < ps) rs = ps;
        break;

    case HZAOC_RCP:
    case HZAOC_RCG:
    case HZAOC_RCN:
        ps = HZA_OPCODE_PRI_SIZE(insn->opcode);
        switch (ps)
        {
        case 0: case 1: case 2: case 3: case 4:
            break;
        case 5:
            if (insn->b >= proc->const32_count)
            {
                E("I$.4Hd: bad 32-bit const index (b = $XUw)",
                  insn - proc->insn_table, insn->b);
                return -1;
            }
            break;
        case 6:
            if (insn->b >= proc->const64_count)
            {
                E("I$.4Hd: bad 64-bit const index (b = $XUw)",
                  insn - proc->insn_table, insn->b);
                return -1;
            }
            break;
        case 7:
            if (insn->b >= proc->const128_count)
            {
                E("I$.4Hd: bad 128-bit const index (b = $XUw)",
                  insn - proc->insn_table, insn->b);
                return -1;
            }
            break;
        }
        break;

    case HZAOC_RAN:
    case HZAOC_RAA:
    case HZAOC_RA4:
    case HZAOC_RA5:
    case HZAOC_RA6:
        ps = 6; // addresses are 2^6 bits
        goto l_check_b_reg;

    default:
        return -1;
    }

    /* check operand c */
    switch (oc)
    {
    case HZAOC_NNN:
    case HZAOC_RNN:
    case HZAOC_RRN:
    case HZAOC_RAN:
    case HZAOC_SRN:
    case HZAOC_RCN:
    case HZAOC_RR4:
    case HZAOC_QR4:
    case HZAOC_RA4:
        break;

    case HZAOC_RRR:
    case HZAOC_QRR:
        ps = 1 << HZA_OPCODE_PRI_SIZE(insn->opcode);
    l_check_c_reg:
        c = insn->c;
        if ((c & (ps - 1)) != 0)
        {
            E("I$.4Hd: unaligned reg (c = $XUw)",
              insn - proc->insn_table, insn->c);
            return -1; // unaligned reg
        }
        ps += c;
        if (rs < ps) rs = ps;
        break;
        break;

    case HZAOC_RRC:
    case HZAOC_QRC:
        ps = HZA_OPCODE_PRI_SIZE(insn->opcode);
        switch (ps)
        {
        case 0: case 1: case 2: case 3: case 4:
            break;
        case 5:
        l_check_c5:
            if (insn->c >= proc->const32_count)
            {
                E("I$.4Hd: bad 32-bit const index (c = $XUw)",
                  insn - proc->insn_table, insn->c);
                return -1;
            }
            break;
        case 6:
        l_check_c6:
            if (insn->c >= proc->const64_count)
            {
                E("I$.4Hd: bad 64-bit const index (c = $XUw)",
                  insn - proc->insn_table, insn->c);
                return -1;
            }
            break;
        case 7:
            if (insn->c >= proc->const128_count)
            {
                E("I$.4Hd: bad 128-bit const index (c = $XUw)",
                  insn - proc->insn_table, insn->c);
                return -1;
            }
            break;
        }
        break;

    case HZAOC_RRS:
    case HZAOC_QRS:
        ps = 1 << HZA_OPCODE_SEC_SIZE(insn->opcode);
        goto l_check_c_reg;

    case HZAOC_RNP:
    case HZAOC_RRP:
    case HZAOC_RCP:
        c = insn->c;
        if (c + 1 >= proc->target_count) return -1;
        break;

    case HZAOC_RRG:
    case HZAOC_RCG:
        c = insn->c;
        if (c + 2 >= proc->target_count) return -1;
        break;

    case HZAOC_RLT:
        b = insn->b;
        c = insn->c;
        if (b + c > proc->target_count)
        {
            E("I$.4d: bad target table ref (b = $XUw, c = $XUw)",
              insn - proc->insn_table, b, c);
            return -1;
        }
        break;

    case HZAOC_RAA:
        ps = 6;
        goto l_check_c_reg;

    case HZAOC_RA5:
        goto l_check_c5;

    case HZAOC_RA6:
        goto l_check_c6;

    default:
        return -1;
    }

    return rs;
}

/* last_insn_check **********************************************************/
static int last_insn_check
(
    hza_insn_t * insn
)
{
    switch (HZA_OPCODE_CLASS(insn->opcode))
    {
    case HZAOC_NNN:
        switch (HZA_OPCODE_FNSZ(insn->opcode))
        {
        case HZA_OPCODE_FNSZ(HZAO_RET):
        case HZA_OPCODE_FNSZ(HZAO_HALT):
            return 0;
        }
        return 1;
    case HZAOC_RNP:
    case HZAOC_RRP:
    case HZAOC_RCP:
    case HZAOC_RRG:
    case HZAOC_RCG:
    case HZAOC_RLT:
        return 0;
    }
    return 1;
}

/* task_alloc ***************************************************************/
static hza_error_t C41_CALL task_alloc
(
    hza_context_t * hc
)
{
    hza_world_t * w = hc->world;
    hza_task_t * t;
    int mae, e;

    mae = c41_ma_alloc_zero_fill(&w->mac.ma, (void * *) &hc->args.task,
                                 sizeof(hza_task_t));
    if (mae)
    {
        E("failed allocating memory for new task (ma error $i)", mae);
        hc->ma_error = mae;
        return hc->hza_error = HZAE_ALLOC;
    }
    t = hc->args.task;

    t->reg_limit = INIT_REG_SIZE;
    mae = c41_ma_realloc_array(&w->mac.ma, (void * *) &t->reg_space,
                               1, t->reg_limit, 0);
    if (mae)
    {
        E("failed allocating register space for new task (ma error $i)", mae);
        hc->ma_error = mae;
        goto l_free;
    }

    t->frame_limit = INIT_FRAME_LIMIT;
    mae = c41_ma_realloc_array(&w->mac.ma, (void * *) &t->frame_table,
                               sizeof(hza_frame_t), t->frame_limit, 0);
    if (mae)
    {
        E("failed allocating frame table for new task (ma error $i)", mae);
        hc->ma_error = mae;
        goto l_free;
    }

    t->module_limit = INIT_MODMAP_LIMIT;
    mae = c41_ma_realloc_array(&w->mac.ma, (void * *) &t->module_table,
                               sizeof(hza_modmap_t), t->module_limit, 0);
    if (mae)
    {
        E("failed allocating module map table for new task (ma error $i)", mae);
        hc->ma_error = mae;
        goto l_free;
    }

    return 0;
l_free:
    e = task_free(hc);
    if (e) return e;
    return hc->hza_error = HZAE_ALLOC;
}

/* task_free ****************************************************************/
static hza_error_t C41_CALL task_free
(
    hza_context_t * hc
)
{
    hza_world_t * w = hc->world;
    hza_task_t * t = hc->args.task;
    int mae;

    if (t->module_table)
    {
        mae = c41_ma_realloc_array(&w->mac.ma, (void * *) &t->module_table,
                                   sizeof(hza_modmap_t), 0, t->module_limit);
        if (mae)
        {
            F("error freeing module map table for failed new task "
              "(ma error $i)", mae);
            hc->ma_free_error = mae;
            return hc->hza_error = HZAF_FREE;
        }
    }

    if (t->frame_table)
    {
        mae = c41_ma_realloc_array(&w->mac.ma, (void * *) &t->frame_table,
                                   sizeof(hza_frame_t), 0, t->frame_limit);
        if (mae)
        {
            F("error freeing frame table for failed new task (ma error $i)",
              mae);
            hc->ma_free_error = mae;
            return hc->hza_error = HZAF_FREE;
        }
    }

    if (t->reg_space)
    {
        mae = c41_ma_realloc_array(&w->mac.ma, (void * *) &t->reg_space,
                                   1, 0, t->reg_limit);
        if (mae)
        {
            F("error freeing register space for failed new task (ma error $i)",
              mae);
            hc->ma_free_error = mae;
            return hc->hza_error = HZAF_FREE;
        }
    }

    mae = c41_ma_free(&w->mac.ma, t, sizeof(hza_task_t));
    if (mae)
    {
        F("error freeing failed new task (ma error $i)", mae);
        hc->ma_free_error = mae;
        return hc->hza_error = HZAF_FREE;
    }
    return 0;
}

/* task_init ****************************************************************/
static hza_error_t C41_CALL task_init
(
    hza_context_t * hc
)
{
    hza_world_t * w = hc->world;
    hza_task_t * t = hc->args.task;

    t->task_id = w->task_id_seed++;
    t->owner = hc;
    t->context_count = 1;
    t->state = HZA_TASK_SUSPENDED;
    C41_DLIST_APPEND(w->task_list[t->state], t, links);

    t->module_table[0].anchor = 0;
    t->module_table[0].module = w->core_module;
    t->module_table[0].task = t;
    t->module_count = 1;

    t->frame_table[0].proc = w->core_module->proc_table + 0;
    t->frame_table[0].insn = w->core_module->proc_table[0].insn_table + 0;
    t->frame_table[0].reg_base = 0;

    c41_dlist_init(&t->context_wait_queue);

    hc->active_task = t;

    return 0;
}

/* hza_task_create **********************************************************/
HAZNA_API hza_error_t C41_CALL hza_task_create
(
    hza_context_t * hc,
    hza_task_t * * tp
)
{
    hza_world_t * w = hc->world;
    hza_task_t * t;
    hza_error_t e;

    e = run_locked(hc, task_alloc, w->world_mutex);
    if (e)
    {
        E("failed allocating memory for a new task ($s = $i)",
          hza_error_name(e), e);
        return e;
    }
    e = run_locked(hc, task_init, w->task_mutex);
    if (e)
    {
        E("failed initing new task ($s = $i)", hza_error_name(e), e);
        if (e >= HZA_FATAL) return e;
        return e;
    }

    *tp = t = hc->args.task;
    D("task t$.4Hd created ($G4Xp)", t->task_id, t);

    return 0;
}

/* hza_enter ****************************************************************/
HAZNA_API hza_error_t C41_CALL hza_enter
(
    hza_context_t * hc,
    uint32_t module_index,
    uint32_t proc_index,
    uint16_t reg_shift
)
{
    hza_task_t * t = hc->active_task;
    hza_module_t * m;
    hza_proc_t * p;
    hza_error_t e;
    uint_t fx;
    uint32_t reg_base, reg_limit;

    DEBUG_CHECK(t);
    DEBUG_CHECK(module_index < t->module_count);
    m = t->module_table[module_index].module;
    DEBUG_CHECK(proc_index < m->proc_count);
    p = &m->proc_table[proc_index];
    reg_base = t->frame_table[t->frame_index].reg_base + (reg_shift >> 3);
    fx = t->frame_index += 1;
    if (fx == t->frame_limit)
    {
        // extend frame table
        e = safe_realloc_table(hc, t->frame_table, sizeof(hza_frame_t),
                               fx << 1, fx);
        if (e)
        {
            E("failed reallocating frame table in task t$H.4d to $Ui items",
              t->task_id, fx << 1);
            t->frame_index -= 1;
            return e;
        }
        t->frame_table = hc->args.realloc.ptr;
        t->frame_limit = fx << 1;
        D("reallocated frame table for t$.4Hd to $Ui items", t->task_id,
          t->frame_limit);
    }
    t->frame_table[fx].proc = p;
    t->frame_table[fx].insn = p->insn_table;
    t->frame_table[fx].module_index = module_index;
    t->frame_table[fx].reg_base = reg_base;
    reg_limit = reg_base + p->reg_size;
    if (reg_limit > t->reg_limit)
    {
        // extend reg space
        uint_t new_reg_limit;
        for (new_reg_limit = t->reg_limit; 
             new_reg_limit < reg_limit; 
             new_reg_limit <<= 1);
        e = safe_realloc_table(hc, t->reg_space, 1, 
                               new_reg_limit, t->reg_limit);
        if (e)
        {
            E("failed reallocating reg space in task t$H.4d to $Ui bytes",
              t->task_id, new_reg_limit);
            return e;
        }
        t->reg_space = hc->args.realloc.ptr;
        t->reg_limit = new_reg_limit;
        D("reallocated reg space for t$.4Hd to $.1Xd bytes", t->task_id,
          new_reg_limit);
    }

    return 0;
}

/* hza_run ******************************************************************/
HAZNA_API hza_error_t C41_CALL hza_run
(
    hza_context_t * hc,
    uint_t frame_stop,
    uint_t iter_limit
)
{
    hza_world_t * w = hc->world;
    hza_task_t * t;
    hza_proc_t * p;
    hza_insn_t * i;
    hza_insn_t * li;
    hza_frame_t * f;
    uint8_t * r;
    uint32_t fx, reg_base;
    uint_t iter_count;
    uint_t target_index;

#define UPDATE_ITER_COUNT() (iter_count += i - li)
#define CHECK_ITER_COUNT() if (UPDATE_ITER_COUNT() >= iter_limit) goto l_done
#define VU8(_bit_ofs) (*(uint8_t *) (r + ((_bit_ofs) >> 3)))
#define VU16(_bit_ofs) (*(uint16_t *) (r + ((_bit_ofs) >> 3)))

    t = hc->active_task;
    DEBUG_CHECK(t);

    fx = t->frame_index;
    if (fx <= frame_stop) { hc->args.iter_count = 0; return 0; }
    f = t->frame_table + fx;
    p = f->proc;
    li = i = f->insn;
    reg_base = f->reg_base;
    r = t->reg_space + reg_base;

    (void) iter_limit;
    (void) p;
    for (iter_count = 0;;)
    {
        D("t$.4Hd M$.4Hd.P$.4Hd.I$.4Hd: $s ($XUw) $XUw $XUw $XUw",
          t->task_id, f->module_index,
          p - t->module_table[f->module_index].module->proc_table,
          i - p->insn_table,
          hza_opcode_name(i->opcode), i->opcode, i->a, i->b, i->c);
        switch (i->opcode)
        {
        case HZAO_NOP:
            break;
        case HZAO_RET:
            UPDATE_ITER_COUNT();
            if (--fx == frame_stop)
            {
                t->frame_index = fx;
                goto l_done;
            }
            --f;
            li = i = f->insn;
            reg_base = f->reg_base;
            r = t->reg_space + reg_base;
            if (iter_count >= iter_limit) goto l_done;
            break;
        case HZAO_INIT_8:
            VU8(i->a) = i->b;
            break;
        case HZAO_INIT_16:
            VU16(i->a) = i->b;
            break;
        case HZAO_DEBUG_OUT_16:
            D("DEBUG_OUT: $c", VU16(i->a));
            if (w->log_level == HZA_LL_INFO && w->log_io)
            {
                c41_io_fmt(w->log_io, "$c", VU16(i->a));
            }
            break;
        case HZAO_WRAP_ADD_CONST_8:
            VU8(i->a) = VU8(i->b) + i->c;
            D("wrap add: $Xb", VU8(i->a));
            break;
        case HZAO_BRANCH_ZERO_8:
            // D("tgt_idx");
            // UPDATE_ITER_COUNT();
            // D("iter_count: $Xi", iter_count);
            if (iter_count >= iter_limit) goto l_done;
            CHECK_ITER_COUNT();
            target_index = i->c + (VU8(i->a) ? 1 : 0);
            D("tgt_idx: $i => $Xd", target_index, p->target_table[target_index]);
            i = p->insn_table + p->target_table[target_index];
            break;
        default:
            F("opcode $s ($XUw) is not implemented!",
              hza_opcode_name(i->opcode), i->opcode);
            return hc->hza_error = HZAF_NO_CODE;
        }
        i++;
    }
l_done:
    hc->args.iter_count = iter_count;

    return 0;
#undef UPDATE_ITER_COUNT
}

