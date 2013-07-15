#ifndef _HZA_H_
#define _HZA_H_

#include <c41.h>

#if HAZNA_STATIC
#   define HAZNA_API
#elif HAZNA_DL_BUILD
#   define HAZNA_API C41_DL_EXPORT
#else
#   define HAZNA_API C41_DL_IMPORT
#endif

/* error codes */
enum hza_error_enum
{
    HZA_OK = 0,

    HZAE_WORLD_ALLOC,
    HZAE_WORLD_FINISH,
    HZAE_LOG_MUTEX_INIT,
    HZAE_ALLOC,
    HZAE_STATE,
    HZAE_STACK_LIMIT,
    HZAE_PROC_INDEX,
    HZAE_MOD00_TRUNC,
    HZAE_MOD00_MAGIC,
    HZAE_MOD00_CORRUPT,

    HZA_FATAL = 0x80,
    HZAF_BUG,
    HZAF_NO_CODE,
    HZAF_MUTEX_LOCK,
    HZAF_MUTEX_UNLOCK,
    HZAF_WORLD_FREE,
    HZAF_FREE,
    HZAF_ALLOC,
    HZAF_OPCODE, // unsupported opcode
};

enum hza_opcode_enum
{
    HZAO_NOP = 0,
    HZAO_HALT,
};

/* log levels */
#define HZA_LL_NONE 0
#define HZA_LL_FATAL 1
#define HZA_LL_ERROR 2
#define HZA_LL_WARNING 3
#define HZA_LL_INFO 4
#define HZA_LL_DEBUG 5

/* task states */
#define HZA_TASK_RUNNING        0
#define HZA_TASK_WAITING        1
#define HZA_TASK_READY          2
#define HZA_TASK_SUSPENDED      3
#define HZA_TASK_STATES         4

#define HZA_MAX_PROC 0x01000000 // 16M procs per module tops! or else...

#define HZA_MOD00_MAGIC "[hza00]\x0A"
#define HZA_MOD00_MAGIC_LEN 8

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
/* hza_mod00_proc_t *********************************************************/
typedef struct hza_mod00_proc_s                 hza_mod00_proc_t;

struct hza_context_s
{
    hza_world_t *               world;
    hza_task_t *                active_task;
    uint_t                      ma_error;
    uint_t                      ma_free_error;
    uint_t                      smt_error;
    hza_error_t                 hza_error;
    hza_error_t                 hza_finish_error;
    union
    {
        uint_t                      context_count;
        struct
        {
            void *                      ptr;
            size_t                      item_size;
            size_t                      new_count;
            size_t                      old_count;
        }                           realloc;
    }                           args;
};

#define HZA_INIT_WORLD_MUTEX    (1 << 0)
#define HZA_INIT_LOG_MUTEX      (1 << 1)
#define HZA_INIT_MODULE_MUTEX   (1 << 2)
#define HZA_INIT_TASK_MUTEX     (1 << 3)

struct hza_world_s
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

struct hza_task_s
{
    c41_np_t                    links;
    uint8_t *                   reg_space;
    hza_frame_t *               frame_table;
    hza_modmap_t *              module_table; // table of imported modules
    hza_context_t *             owner; // the context manipulating the task
    uint_t                      reg_limit; // limit in bytes
    uint_t                      frame_index;
    uint_t                      frame_limit;
    uint_t                      module_count;
    uint_t                      module_limit;
    uint32_t                    task_id; // unique id
    uint32_t                    context_count;
    uint8_t                     state; // which queue this task is in
    uint8_t                     kill_req; // kill requested but task is running
};

struct hza_frame_s
{
    hza_proc_t *                proc;
    hza_insn_t *                insn;
    uint32_t                    reg_base;
        /*< Must store as offset, not pointer, because the reg_space can be
         *  reallocated */
};

struct hza_modmap_s
{
    uint64_t                    anchor;
    /*< usually has a pointer to where the globals are mapped in the
     *  task's memory space */
    hza_module_t *              module;
    hza_task_t *                task;
    //uint32_t index; // the index in task's module table
};

struct hza_uint128_s
{
    uint64_t low, high;
};

struct hza_mod00_hdr_s
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
    /* 0x28 */  uint32_t    target_block_count;
    /* 0x2C */  uint32_t    target_count;
    /* 0x30 */  uint32_t    insn_count;
    /* 0x34 */  uint32_t    data_size;
    /* 0x38 */  uint32_t    reserved0;
    /* 0x3C */  uint32_t    reserved1;
    /* 0x40 */

    // hza_uint128_t[const128_count]            const128_table
    // uint64_t[const64_count]                  const64_table
};

struct hza_mod00_proc_s
{
    uint32_t    insn_start;
    uint32_t    target_block_start;
    uint32_t    const32_start;
    uint32_t    const64_start;
    uint32_t    const128_start;
    uint32_t    name; // data block index
};

struct hza_mod_name_cell_s
{
    // c41_rbtree_node_t rbtn;
    hza_module_t * module;
    uint_t len;
    uint8_t name[1];
};

struct hza_module_s
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
    uint32_t * target_block_start_table; // [target_block_count + 1]
    hza_context_t * owner;

    uint32_t const128_count;
    uint32_t const64_count;
    uint32_t const32_count;
    uint32_t proc_count;
    uint32_t insn_count;
    uint32_t target_count;
    uint32_t target_block_count;
    uint32_t data_block_count;
    uint32_t data_size;

    uint32_t module_id;
    uint32_t task_count; // number of tasks that have imported this module
};

struct hza_proc_s
{
    hza_insn_t * insn_table;
    uint32_t * const32_table;
    uint64_t * const64_table;
    hza_uint128_t * const128_table;
    uint32_t * target_block_start_table;
    uint32_t insn_start; // index in module->insn_table to insn that start execution
    uint32_t const32_start;
    uint32_t const64_start;
    uint32_t const128_start;
    uint32_t target_block_start; // here is where targets are picked from
    uint16_t reg_size; // size of proc's register space (in bits)
};

struct hza_insn_s
{
    uint16_t opcode;
    uint16_t a, b, c;
};

/* hza_lib_name *************************************************************/
HAZNA_API char const * C41_CALL hza_lib_name ();

/* hza_error_name ***********************************************************/
HAZNA_API char const * C41_CALL hza_error_name (hza_error_t e);

/* hza_init *****************************************************************/
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

/* hza_attach ***************************************************************/
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

/* hza_finish ***************************************************************/
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

#endif /* _HZA_H_ */

