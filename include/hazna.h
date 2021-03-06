#ifndef _HZA_H_
#define _HZA_H_

/* prologue {{{1 */
#include <c41.h>

#if HAZNA_STATIC
#   define HAZNA_API
#elif HAZNA_DL_BUILD
#   define HAZNA_API C41_DL_EXPORT
#else
#   define HAZNA_API C41_DL_IMPORT
#endif

/* error codes {{{1 */
enum hza_error_enum
{
    HZA_OK = 0,

    HZAE_WORLD_ALLOC,
    HZAE_WORLD_FINISH,
    HZAE_LOG_MUTEX_INIT,
    HZAE_ALLOC,
    HZAE_STATE,
    HZAE_STACK_LIMIT,
    HZAE_REG_LIMIT,
    HZAE_PROC_INDEX,
    HZAE_MOD00_TRUNC,
    HZAE_MOD00_MAGIC,
    HZAE_MOD00_CORRUPT,
    HZAE_COND_CREATE,

    HZA_FATAL = 0x80,
    HZAF_BUG,
    HZAF_NO_CODE,
    HZAF_MUTEX_LOCK,
    HZAF_MUTEX_UNLOCK,
    HZAF_COND_DESTROY,
    HZAF_WORLD_FREE,
    HZAF_FREE,
    HZAF_ALLOC,
    HZAF_OPCODE, // unsupported opcode
};

/* init flags {{{1 */
#define HZA_INIT_WORLD_MUTEX                    (1 << 0)
#define HZA_INIT_LOG_MUTEX                      (1 << 1)
#define HZA_INIT_MODULE_MUTEX                   (1 << 2)
#define HZA_INIT_TASK_MUTEX                     (1 << 3)

/* operand types {{{1 */
enum hza_operand_types
{
    HZAOT_N, // none
    HZAOT_R, // register depending on primary size (stored in opcode)
    HZAOT_Q, // register with double the primary size
    HZAOT_S, // register with secondary size
    HZAOT_A, // 64-bit register (used as address)
    HZAOT_C, // constant of primary size
    HZAOT_4, // 16-bit const offset/index
    HZAOT_5, // 32-bit const offset/index
    HZAOT_6, // 64-bit const offset/index
    HZAOT_P, // target-pair for boolean jumps
    HZAOT_G, // signum triplet of targets (less than, equal, grater than)
    HZAOT_L, // target-table length
    HZAOT_T, // target-table index
};

/* opcode classes {{{1 */
#define HZAOC_NNN 0x00 /* nop, halt */
#define HZAOC_RNN 0x01 /* in, out, debug_out */
#define HZAOC_RRN 0x02 /* unary ops (neg, not) */
#define HZAOC_RRR 0x03 /* binary ops: add/sub/or/xor... */
#define HZAOC_QRR 0x04 /* binary ops where result is double-size (add, mul) */
#define HZAOC_RRC 0x05 /* binary ops with const */
#define HZAOC_QRC 0x06 /* bin ops with result double-size */
#define HZAOC_SRN 0x07 /* zero-extend, sign-extend */
#define HZAOC_RRS 0x08 /* shift */
#define HZAOC_QRS 0x09 /* shift */
#define HZAOC_RR4 0x0A /* shift */
#define HZAOC_QR4 0x0B /* shift */
#define HZAOC_RCN 0x0C /* init with const */
#define HZAOC_RNP 0x0D /* jump if reg is zero/non-zero */
#define HZAOC_RRP 0x0E /* cmp reg, reg and jump */
#define HZAOC_RCP 0x0F /* cmp reg, const and jump */
#define HZAOC_RRG 0x10 /* cmp-and-jump reg, reg with 3 targets (< = >) */
#define HZAOC_RCG 0x11 /* cmp-and-jump reg, ct with 3 target (< = >) */
#define HZAOC_RLT 0x12 /* table-jump (packed-switch) */
#define HZAOC_RAN 0x13 /* load/store from address */
#define HZAOC_RAA 0x14 /* load/store from addr_reg + offset_reg|index_reg */
#define HZAOC_RA4 0x15 /* load/store from addr_reg + 16bit-displacement */
#define HZAOC_RA5 0x16 /* load/store from addr_reg + 32bit-displacement */
#define HZAOC_RA6 0x17 /* load/store from addr_reg + 64bit-displacement */
#define HZAOC_x18
#define HZAOC_x19
#define HZAOC_x1A
#define HZAOC_x1B
#define HZAOC_x1C
#define HZAOC_x1D
#define HZAOC_x1E
#define HZAOC_x1F

/* opcode sizes {{{1 */
#define HZAS_1 0
#define HZAS_2 1
#define HZAS_4 2
#define HZAS_8 3
#define HZAS_16 4
#define HZAS_32 5
#define HZAS_64 6
#define HZAS_128 7

/* opcode macros {{{1 */
#define HZA_OPCODE(_class, _index) (((_class) << 11) | (_index))
#define HZA_OPCODE1(_class, _pri_size, _fn) \
    (HZA_OPCODE((_class), ((_pri_size) << 8) | (_fn)))
#define HZA_OPCODE2(_class, _pri_size, _sec_size, _fn) \
    (HZA_OPCODE((_class), ((_pri_size) << 8) | ((_sec_size) << 5) | (_fn)))
#define HZA_OPCODE_CLASS(_opcode) ((_opcode) >> 11)
#define HZA_OPCODE_PRI_SIZE(_opcode) (((_opcode) >> 8) & 7)
#define HZA_OPCODE_SEC_SIZE(_opcode) (((_opcode) >> 5) & 7)
#define HZA_OPCODE_FNSZ(_opcode) ((_opcode) & 0x7FF)

/* opcodes {{{1 */

/* nnn */
#define HZAO_NOP                HZA_OPCODE(HZAOC_NNN, 0x000)
#define HZAO_HALT               HZA_OPCODE(HZAOC_NNN, 0x001)
#define HZAO_RET                HZA_OPCODE(HZAOC_NNN, 0x002)

/* rnn */
#define HZAO_DEBUG_OUT_16       HZA_OPCODE1(HZAOC_RNN, HZAS_16, 0x000)
#define HZAO_DEBUG_OUT_32       HZA_OPCODE1(HZAOC_RNN, HZAS_32, 0x000)

/* rnc */
#define HZAO_INIT_8             HZA_OPCODE1(HZAOC_RCN, HZAS_8, 0x000)
#define HZAO_INIT_16            HZA_OPCODE1(HZAOC_RCN, HZAS_16, 0x000)

/* rrc */
#define HZAO_WRAP_ADD_CONST_8   HZA_OPCODE1(HZAOC_RRC, HZAS_8, 0x0000)

/* rnp */
#define HZAO_BRANCH_ZERO_1      HZA_OPCODE1(HZAOC_RNP, HZAS_1, 0x000)
#define HZAO_BRANCH_ZERO_2      HZA_OPCODE1(HZAOC_RNP, HZAS_2, 0x000)
#define HZAO_BRANCH_ZERO_4      HZA_OPCODE1(HZAOC_RNP, HZAS_4, 0x000)
#define HZAO_BRANCH_ZERO_8      HZA_OPCODE1(HZAOC_RNP, HZAS_8, 0x000)
#define HZAO_BRANCH_ZERO_16     HZA_OPCODE1(HZAOC_RNP, HZAS_16, 0x000)
#define HZAO_BRANCH_ZERO_32     HZA_OPCODE1(HZAOC_RNP, HZAS_32, 0x000)
#define HZAO_BRANCH_ZERO_64     HZA_OPCODE1(HZAOC_RNP, HZAS_64, 0x000)
#define HZAO_BRANCH_ZERO_128    HZA_OPCODE1(HZAOC_RNP, HZAS_128, 0x000)


/* log levels {{{1 */
#define HZA_LL_NONE 0
#define HZA_LL_FATAL 1
#define HZA_LL_ERROR 2
#define HZA_LL_WARNING 3
#define HZA_LL_INFO 4
#define HZA_LL_DEBUG 5

/* task states {{{1 */
#define HZA_TASK_RUNNING        0
#define HZA_TASK_WAITING        1
#define HZA_TASK_READY          2
#define HZA_TASK_SUSPENDED      3
#define HZA_TASK_STATES         4

/* other constants {{{1 */
#define HZA_MAX_PROC 0x01000000 // 16M procs per module tops! or else...

#define HZA_MOD00_MAGIC "[hza00]\x0A"
#define HZA_MOD00_MAGIC_LEN 8

/* forward type declarations {{{1 */
/* hza_error_t **************************************************************/
/**
 * Error code.
 * This is an integer type.
 * Most lib functions return this.
 */
typedef unsigned int                            hza_error_t;

/* hza_uint128_t ************************************************************/
typedef struct hza_uint128_s                    hza_uint128_t;

/* hza_context_t ************************************************************/
/**
 * Context data: Each native thread working with a world has one context.
 * This is used to hold thread-local vars (like error codes from mem allocator,
 * multithreading interface, etc)
 */
typedef struct hza_context_s                    hza_context_t;

/* hza_world_t **************************************************************/
/**
 * The world that contains tasks and modules.
 */
typedef struct hza_world_s                      hza_world_t;

/* hza_task_t ***************************************************************/
/**
 * A linear execution unit.
 */
typedef struct hza_task_s                       hza_task_t;

/* hza_frame_t *******************************************************/
/**
 * The item of a task's call stack.
 */
typedef struct hza_frame_s                      hza_frame_t;

/* hza_modmap_t ******************************************************/
/**
 * Contains data specific to a module imported into a task.
 */
typedef struct hza_modmap_s                     hza_modmap_t;

/* hza_module_t *************************************************************/
/**
 * Module data: contains its code and static data
 */
typedef struct hza_module_s                     hza_module_t;

/* hza_mod_name_cell_t *******************************************************/
typedef struct hza_mod_name_cell_s              hza_mod_name_cell_t;

/* hza_proc_t ***************************************************************/
/**
 * Describes one procedure.
 */
typedef struct hza_proc_s                       hza_proc_t;

/* hza_insn_t ***************************************************************/
/**
 * One instruction: opcode a, b, c.
 */
typedef struct hza_insn_s                       hza_insn_t;

/* hza_mod00_hdr_t **********************************************************/
typedef struct hza_mod00_hdr_s                  hza_mod00_hdr_t;

/* hza_mod00_impmod_t *******************************************************/
typedef struct hza_mod00_impmod_s               hza_mod00_impmod_t;

/* hza_mod00_proc_t *********************************************************/
typedef struct hza_mod00_proc_s                 hza_mod00_proc_t;

struct hza_context_s /* hza_context_t {{{1 */
{
    hza_world_t *               world;
    hza_task_t *                active_task;
    c41_smt_cond_t *            cond;
    uint_t                      ma_error;
    uint_t                      ma_free_error;
    uint_t                      smt_error;
    hza_error_t                 hza_error;
    hza_error_t                 hza_finish_error;
    union
    {
        uint_t                      context_count;
        uint32_t                    module_index;
        struct
        {
            void *                      ptr;
            size_t                      item_size;
            size_t                      new_count;
            size_t                      old_count;
        }                           realloc;
        hza_task_t *                task;
        uint_t                  iter_count;
    }                           args;
};

struct hza_world_s /* hza_world_t {{{1 */
{
    c41_np_t                    task_list[HZA_TASK_STATES];
        /*< Task queues.
            Access these with #task_mutex locked!
            */
    c41_np_t                    module_list;
        /*< Loaded modules list.
         *  These modules have been processes/optimised/JITed and so on.
         *  No changes are allowed to these.
         *  They can be imported in tasks.
         */
    c41_rbtree_t                module_name_tree;
        /*< Mapping name->module.
         */

    hza_module_t *              core_module;

    c41_smt_t *                 smt; // multithreading interface
        /*< Multithreading interface.
         *  This provides functions to create/destroy/lock/unlock mutexes.
         */
    c41_smt_mutex_t *           task_mutex; // task manager mutex
        /*< Mutex for global changes to any task.
         */
    c41_smt_mutex_t *           module_mutex; // module manager mutex
        /*< Mutex for global changes to modules.
         */
    c41_smt_mutex_t *           world_mutex;
        /*< Mutex for global changes to contexts and for memory allocation.
         */
    c41_smt_mutex_t *           log_mutex;
        /*< Mutex used by logging. Some messages are printed in few distinct
         *  writes to the io stream which can cause interweaving of messages
         *  when writing from multiple threads.
         */

    c41_ma_counter_t            mac;
        /*< Memory allocator used for all resouces managed by a world.
         *  This allocator counts blocks and total size to enable detection
         *  of memory leaks. Counting is not multithreading safe, therefore
         *  any operation performed on the memory allocator has to be protected
         *  by #world_mutex.
         */
    c41_io_t *                  log_io;
        /*< Logging I/O stream.
         */
    c41_ma_t *                  world_ma;
        /*< Memory allocator used to allocate this structure.
         *  This is the original allocator passed to hza_init().
         *  This allocator is wrapped in a counter memory allocator and stored
         *  in #mac to be used for resources linked to this world.
         */
    uint_t                      task_id_seed;
        /*< Seed value for task IDs. */
    uint_t                      module_id_seed;
        /*< Seed value for module IDs. */
    uint_t                      context_count;
        /*< Number of contexts attached to this world */
    uint16_t                    init_state;
        /*< Bitmask of what fields are initialised.
         *  This helps hza_finish() to destroy the world when all contexts
         *  abandon the world.
         */
    uint8_t                     log_level;
        /*< Log verbosity.
         *  Possible values are defined as HZA_LL_xxx.
         *  On release builds, level HZA_LL_DEBUG is downgraded to HZA_LL_INFO.
         */
};

struct hza_task_s /* hza_task_t {{{1 */
{
    c41_np_t                    links; /**<
                                    entry for doubly-linked list corresponding
                                    to task's state;
                                    this should be accessed while holding the
                                    task mutex.
                                    */
    uint8_t *                   reg_space; /**<
                                    pointer to the register/local space;
                                    only the context owning the task should 
                                    access this;
                                    */
    hza_frame_t *               frame_table; /**<
                                    table of frames referencing the next
                                    instruction to be executed in each procedure
                                    that started execution;
                                    only the context owning the task should 
                                    access this;
                                    */
    hza_modmap_t *              module_table; /**<
                                    table of imported modules;
                                    */
    hza_context_t *             owner; /**<
                                    the context manipulating the task_id
                                    */
    uint_t                      reg_limit; /**<
                                    number of bytes allocated for reg_space */
    uint_t                      frame_index; /**<
                                    index of the frame executing current
                                    instruction */
    uint_t                      frame_limit; /**<
                                    number of frames allocated in frame_table;
                                    */
    uint_t                      module_count; /**<
                                    number of modules imported
                                    */
    uint_t                      module_limit; /**<
                                    number of items allocated in module_table
                                    */
    uint32_t                    task_id; /**<
                                    unique id throughout the life of the world
                                    */
    uint32_t                    context_count; /**<
                                    number of contexts holding a pointer to
                                    this task
                                    */
    uint8_t                     state; /**<
                                    current state of the task;
                                    this determines which queue this task is in
                                    */
    uint8_t                     kill_req; /**<
                                    kill requested but task is running;
                                    this will be replaced later with some sort
                                    of signal mask where one signal is for kill
                                    */

    c41_np_t                    context_wait_queue; /**<
                                    linked list of contexts waiting to attach
                                    this task
                                    */
};

struct hza_frame_s /* hza_frame_t {{{1 */
{
    hza_proc_t *                proc;
    hza_insn_t *                insn;
    uint32_t                    module_index; // task-local mod mapping index
    uint32_t                    reg_base;
        /*< Must store as offset, not pointer, because the reg_space can be
         *  reallocated; the offset is in bytes and is multiple of largest
         *  register size (128-bits = 16 bytes) 
         **/
};

struct hza_modmap_s /* hza_modmap_t {{{1 */
{
    uint64_t                    anchor;
    /*< usually has a pointer to where the globals are mapped in the
     *  task's memory space */
    hza_module_t *              module;
    hza_task_t *                task;
    //uint32_t index; // the index in task's module table
};

struct hza_uint128_s /* hza_uint128_t {{{1 */
{
    uint64_t low, high;
};

struct hza_mod00_hdr_s /* hza_mod00_hdr_t {{{1 */
{
    /* 0x00 */  uint8_t     magic[HZA_MOD00_MAGIC_LEN];
    /* 0x08 */  uint32_t    size;
    /* 0x0C */  uint32_t    checksum;
    /* 0x10 */  uint32_t    name;
    /* 0x14 */  uint32_t    const128_count;
    /* 0x18 */  uint32_t    const64_count;
    /* 0x1C */  uint32_t    const32_count;
    /* 0x20 */  uint32_t    proc_count;
    /* 0x24 */  uint32_t    data_block_count;
    /* 0x28 */  uint32_t    import_module_count;
    /* 0x2C */  uint32_t    import_count;
    /* 0x30 */  uint32_t    export_count;
    /* 0x34 */  uint32_t    target_count;
    /* 0x38 */  uint32_t    insn_count;
    /* 0x3C */  uint32_t    data_size;
    /* 0x40 - size of header */
/*
 * mod00 format:
 *  header
 *  const128    const128_count * 16 bytes
 *  const64     const64_count * 8 bytes
 *  const32     const32_count * 4 bytes
 *  proc        (proc_count + 1) * sizeof(mod00_proc)
 *  data_block  (data_block_count + 1) * 4 bytes
 *  imp_mod
 *  imp_proc
 *  export
 *  target      target_count * 4 bytes
 *  insn        insn_count * 8 bytes
 *  data        data_size bytes
 */

};

struct hza_mod00_proc_s /* hza_mod00_proc_t {{{1 */
{
    uint32_t    insn_start;
    uint32_t    target_start;
    uint32_t    const128_start;
    uint32_t    const64_start;
    uint32_t    const32_start;
    uint32_t    name; // data block index
};

struct hza_mod00_impmod_s /* hza_mod00_impmod_t {{{1 */
{
    uint32_t    name;
    uint32_t    proc_start; // start index in import proc area 
};

struct hza_mod_name_cell_s /* hza_mod_name_cell_t {{{1 */
{
    // c41_rbtree_node_t rbtn;
    hza_module_t * module;
    uint_t len;
    uint8_t name[1];
};

struct hza_module_s /* hza_module_t {{{1 */
{
    c41_np_t links;
    hza_proc_t * proc_table;
    hza_insn_t * insn_table;
    uint32_t * target_table;
    hza_uint128_t * const128_table;
    uint64_t * const64_table;
    uint32_t * const32_table;
    uint8_t * data;
    uint32_t * data_block_start_table; // [data_block_count + 1]
    uint32_t * export_table; // table of proc indexes
    hza_context_t * owner;

    uint32_t const128_count;
    uint32_t const64_count;
    uint32_t const32_count;
    uint32_t proc_count;
    uint32_t insn_count;
    uint32_t target_count;
    uint32_t data_block_count;
    uint32_t data_size;
    uint32_t export_count;

    uint32_t module_id;
    uint32_t module_count; // number of modules that import this module
    uint32_t task_count; // number of tasks that have imported this module
    uint32_t ctx_count; // number of contexts holding a pointer to this
    size_t size; // size in memory
};

struct hza_proc_s /* hza_proc_t {{{1 */
{
    hza_insn_t * insn_table;
    hza_uint128_t * const128_table;
    uint64_t * const64_table;
    uint32_t * const32_table;
    uint32_t * target_table;
    uint32_t insn_count;
    uint32_t const128_count;
    uint32_t const64_count;
    uint32_t const32_count;
    uint32_t target_count;
    uint32_t name;
    uint16_t reg_size; // size of proc's register space (in bytes)
};

struct hza_insn_s /* hza_insn_t {{{1 */
{
    uint16_t opcode;
    uint16_t a, b, c;
};

/* hza_lib_name ****************************************************** {{{1 */
HAZNA_API char const * C41_CALL hza_lib_name ();

/* hza_error_name **************************************************** {{{1 */
HAZNA_API char const * C41_CALL hza_error_name (hza_error_t e);

/* hza_init ********************************************************** {{{1 */
/**
 * Initialises a context and the world.
 * Returns:
 *  0 = HZA_OK                  success
 */
HAZNA_API hza_error_t C41_CALL hza_init
(
    hza_context_t * hc,
    c41_ma_t * ma,
    c41_smt_t * smt,
    c41_io_t * log_io,
    uint8_t log_level
);

/* hza_attach ******************************************************** {{{1 */
/**
 * Initialises a context and attaches it to an existing world.
 * Returns:
 *  0 = HZA_OK                  success
 */
HAZNA_API hza_error_t C41_CALL hza_attach
(
    hza_context_t * hc,
    hza_world_t * w
);

/* hza_finish ******************************************************** {{{1 */
/**
 * Finishes one context.
 * If this is the last context attached to the world then it will destroy
 * the world!
 * Returns:
 *  0 = HZA_OK                  success
 */
HAZNA_API hza_error_t C41_CALL hza_finish
(
    hza_context_t * hc
);

/* hza_task_create *************************************************** {{{1 */
/**
 *  Creates a task and attaches it to current context.
 *  The task is created in suspended state and has context_count set to 1.
 */
HAZNA_API hza_error_t C41_CALL hza_task_create
(
    hza_context_t * hc,
    hza_task_t * * tp
);

/* hza_task_ref ****************************************************** {{{1 */
/**
 *  Adds a reference to the given task.
 *  Returns:
 *      0 = HZA_OK              success
 *      HZAF_MUTEX_LOCK
 *      HZAF_MUTEX_UNLOCK
 *      HZAF_BUG                ref count overflows (only in debug builds)
 */
HAZNA_API hza_error_t C41_CALL hza_task_ref
(
    hza_context_t * hc,
    hza_task_t * t
);

/* hza_task_deref **************************************************** {{{1 */
/**
 *  Removes a reference from the given task.
 *  The pointer t can be invalid after this function returns if the task had
 *  only 1 reference.
 *  Returns:
 *      0 = HZA_OK              success
 *      HZAF_MUTEX_LOCK
 *      HZAF_MUTEX_UNLOCK
 *      HZAF_BUG                ref count overflows (only in debug builds)
 */
HAZNA_API hza_error_t C41_CALL hza_task_deref
(
    hza_context_t * hc,
    hza_task_t * t
);

/* hza_task_attach *************************************************** {{{1 */
/**
 *  Attaches the given task to the current context.
 *  This call is needed to perform certain operations on the task and to
 *  start executing it.
 *  If the task is attached to some other context this function will block
 *  until the task is detached and grabbed by the current context.
 */
HAZNA_API hza_error_t C41_CALL hza_task_attach
(
    hza_context_t * hc,
    hza_task_t * t
);

/* hza_task_detach *************************************************** {{{1 */
/**
 *  Detaches the attached task from the current context, notifying if necessary
 *  other contexts waiting to attach that task.
 *  This call keeps the reference to the task, so the caller must also call
 *  hza_task_deref() when done with the task.
 */
HAZNA_API hza_error_t C41_CALL hza_task_detach
(
    hza_context_t * hc
);

/* hza_module_by_name ************************************************ {{{1 */
HAZNA_API hza_error_t C41_CALL hza_module_by_name
(
    hza_context_t * hc,
    uint8_t const * name,
    size_t name_len,
    hza_module_t * * mp
);

/* hza_module_load *************************************************** {{{1 */
HAZNA_API hza_error_t C41_CALL hza_module_load
(
    hza_context_t * hc,
    uint8_t const * data,
    size_t size,
    hza_module_t * * mp
);

/* hza_module_map_name *********************************************** {{{1 */
HAZNA_API hza_error_t C41_CALL hza_module_map_name
(
    hza_context_t * hc,
    hza_module_t * m,
    uint8_t const * name,
    size_t name_len
);

/* hza_export_by_name ************************************************ {{{1 */
/**
 * Searches the exports of a module for the given name.
 * Returns: -1 if name not found, >= 0 the index of the proc found.
 */
HAZNA_API int32_t C41_CALL hza_export_by_name
(
    hza_module_t * m,
    uint8_t const * name,
    size_t name_len
);

/* hza_import ******************************************************** {{{1 */
/**
 * Takes a loaded module and imports it into the task attached to current
 * context.
 * On success it returns 0 and the index of the imported module is stored in
 * hc->args.module_index
 **/
HAZNA_API hza_error_t C41_CALL hza_import
(
    hza_context_t * hc,
    hza_module_t * m,
    uint64_t anchor
);

/* hza_enter ********************************************************* {{{1 */
/**
 *  Pushes a new frame in the stack of the attached task.
 *  This can be done only to attached tasks to ensure there is no concurency
 *  issue with some other context potentially executing the same task.
 *  Parameters:
 *      reg_shift               number of bits to preserve from the caller
 *                              register space; must be a multiple of
 *                              largest reg size (128)
 */
HAZNA_API hza_error_t C41_CALL hza_enter
(
    hza_context_t * hc,
    uint32_t module_index,
    uint32_t proc_index,
    uint16_t reg_shift
);

/* hza_run *********************************************************** {{{1 */
/**
 *  Executes code in the attached task until the given frame is reached or
 *  at least iter_count instructions have been executed.
 *  The iteration count is updated only on certain instructions (usually
 *  those that change the flow) so execution will likely not stop after exactly 
 *  iter_count iterations.
 */
HAZNA_API hza_error_t C41_CALL hza_run
(
    hza_context_t * hc,
    uint_t frame_stop,
    uint_t iter_limit
);
/* }}}1 */

#endif /* _HZA_H_ */

