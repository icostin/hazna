#ifndef _HZA_H_
#define _HZA_H_

#include <c41.h>

#if HZA_STATIC
#   define HZA_API
#elif HZA_DL_BUILD
#   define HZA_API C41_DL_EXPORT
#else
#   define HZA_API C41_DL_IMPORT
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

    HZA_FATAL = 0x80,
    HZAF_BUG,
    HZAF_NO_CODE,
    HZAF_MUTEX_LOCK,
    HZAF_MUTEX_UNLOCK,
    HZAF_WORLD_FREE,
    HZAF_FREE,
    HZAF_ALLOC,
};

enum hza_opcode_enum
{
    HZAO_NOP = 0,
    HZAO_RETURN,
    HZAO_OUTPUT_DEBUG_CHAR, /* output-debug-char r32:aaaa */

    /* const-N  rN:aaaa, CONST_POOL:(bbbb | (cccc << 16)) */
    HZAO_CONST_1 = 8,
    HZAO_CONST_2,
    HZAO_CONST_4,
    HZAO_CONST_8,
    HZAO_CONST_16,
    HZAO_CONST_32,
    HZAO_CONST_64,
    HZAO_CONST_128,

    /* zxconst-N rN:aaaa, zero_extend(bbbb | (cccc << 16)) */
    HZAO_ZXCONST_1,
    HZAO_ZXCONST_2,
    HZAO_ZXCONST_4,
    HZAO_ZXCONST_8,
    HZAO_ZXCONST_16,
    HZAO_ZXCONST_32,
    HZAO_ZXCONST_64,
    HZAO_ZXCONST_128,
    /* sxconst-N rN:aaaa, sign_extend(bbbb | (cccc << 16)) */
    HZAO_SXCONST_1,
    HZAO_SXCONST_2,
    HZAO_SXCONST_4,
    HZAO_SXCONST_8,
    HZAO_SXCONST_16,
    HZAO_SXCONST_32,
    HZAO_SXCONST_64,
    HZAO_SXCONST_128,
};

/* log levels */
#define HZA_LL_NONE 0
#define HZA_LL_FATAL 1
#define HZA_LL_ERROR 2
#define HZA_LL_WARNING 3
#define HZA_LL_INFO 4
#define HZA_LL_DEBUG 5

/* task states */
#define HZA_TASK_RUNNING 0
#define HZA_TASK_READY 1
#define HZA_TASK_SUSPENDED 2
#define HZA_TASK_STATES 3

#define HZA_MAX_PROC 0x1000000 // 16M procs per module tops! or else...

/* hza_error_t **************************************************************/
/**
 * Error code.
 * This is an integer type.
 * Most lib functions return this.
 */
typedef unsigned int                            hza_error_t;

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

/* hza_exec_state_t *******************************************************/
/**
 * The item of a task's call stack.
 */
typedef struct hza_exec_state_s                 hza_exec_state_t;

/* hza_imported_module_t ******************************************************/
/**
 * Contains data specific to a module imported into a task.
 */
typedef struct hza_imported_module_s            hza_imported_module_t;

/* hza_module_t *************************************************************/
/**
 * Module data: contains its code and static data
 */
typedef struct hza_module_s                     hza_module_t;

/* hza_proc_t ***************************************************************/
/**
 * Code for one procedure.
 */
typedef struct hza_proc_s                       hza_proc_t;

/* hza_block_t **************************************************************/
/**
 * Instruction block.
 */
typedef struct hza_block_s                      hza_block_t;

/* hza_insn_t ***************************************************************/
/**
 * One instruction: opcode a, b, c
 */
typedef struct hza_insn_s                       hza_insn_t;

struct hza_context_s
{
    hza_world_t * world;
    hza_task_t * active_task;
    uint_t ma_error;
    uint_t ma_free_error;
    uint_t smt_error;
    hza_error_t hza_error;
    hza_error_t hza_finish_error;
    intptr_t args[4]; // arguments for some internal callback functions
};

#define HZA_INIT_WORLD_MUTEX    (1 << 0)
#define HZA_INIT_LOG_MUTEX      (1 << 1)
#define HZA_INIT_MODULE_MUTEX   (1 << 2)
#define HZA_INIT_TASK_MUTEX     (1 << 3)

struct hza_world_s
{
    c41_np_t task_list[HZA_TASK_STATES];
    /*< Task queues.
        Access these with #task_mutex locked!
        */
    c41_np_t loaded_module_list;
    /*< Loaded modules list.
     *  These modules have been processes/optimised/JITed and so on.
     *  No changes are allowed to these.
     *  They can be imported in tasks.
     */
    c41_np_t unbound_module_list;
    c41_smt_mutex_t * task_mutex; // task manager mutex
    c41_smt_mutex_t * module_mutex; // module manager mutex
    c41_smt_t * smt; // multithreading interface
    c41_ma_counter_t mac;
    c41_ma_t * world_ma;
    c41_smt_mutex_t * world_mutex;
    c41_smt_mutex_t * log_mutex;
    c41_io_t * log_io;
    uint_t task_id_seed;
    uint_t module_id_seed;
    uint_t context_count; // number of contexts attached to the world
    uint16_t init_state;
    uint8_t log_level;
};

struct hza_task_s
{
    c41_np_t links;
    hza_exec_state_t * exec_stack;
    hza_imported_module_t * imp_mod; // table of imported modules
    hza_context_t * owner; // the context manipulating the task
    uint_t stack_depth;
    uint_t stack_limit;
    uint32_t imp_count; // number of imported modules
    uint32_t task_id; // unique id
    // uint32_t ref_count; // how many contexts use this task
    uint8_t state; // which queue this task is in
    uint8_t kill_req; // kill requested but task is running
};

struct hza_exec_state_s
{
    uint16_t target_index; // which exit from the block should it take
    uint16_t module_index; // which module is execution in
    uint32_t block_index; // which block is execution in
    uint16_t insn_index; // which insn is execution at
    uint16_t reg_delta; // bits register data shifted
};

struct hza_imported_module_s
{
    uint64_t anchor;            /* usually has a pointer to where the globals
                                   are mapped in the task's memory space */
    hza_module_t * module;
    hza_task_t * task;
    uint32_t index; // the index in task's module table
};

struct hza_module_s
{
    c41_np_t links;
    hza_proc_t * proc_table;
    hza_block_t * block_table;
    hza_insn_t * insn_table;
    uint32_t * target_table;
    hza_context_t * owner;

    uint32_t proc_count, proc_limit;
    uint32_t proc_unsealed; // index of the proc
    uint32_t block_count, block_limit;
    uint32_t block_unused;
    uint32_t block_unsealed;
    uint32_t insn_count, insn_limit;
    uint32_t insn_unused;
    uint32_t target_count, target_limit;
    uint32_t target_unused;
    uint32_t module_id;
    uint32_t import_count; // number of tasks that have imported this module
};

struct hza_proc_s
{
    uint32_t block_index; // index of first block
    uint32_t block_count;
    uint16_t reg_size; // size of proc's register space (in bits)
};

struct hza_block_s
{
    uint32_t insn_index;
    uint32_t insn_count;
    uint32_t target_index;
    uint32_t target_count;
    uint32_t exc_target; // block index for the exception handler; 0 for unhandled
    uint32_t proc_index; // index of proc containing this block
};

struct hza_insn_s
{
    uint16_t opcode;
    uint16_t a, b, c;
};

/* hza_lib_name *************************************************************/
HZA_API char const * C41_CALL hza_lib_name ();

/* hza_error_name ***********************************************************/
HZA_API char const * C41_CALL hza_error_name (hza_error_t e);

/* hza_init *****************************************************************/
/**
 * Initialises a context and the world.
 * Returns:
 *  0 = HZA_OK                  success
 */
HZA_API hza_error_t C41_CALL hza_init
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
HZA_API hza_error_t C41_CALL hza_attach
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
HZA_API hza_error_t C41_CALL hza_finish
(
    hza_context_t * hc
);

/* hza_create_module ********************************************************/
/**
 * Allocates and initialises an empty module.
 * The module will be inserted in hza_world_t.unbound_module_list and is
 * owned by the current context. The caller must evetually release it in
 * the wild by calling hza_release_module().
 * Returns:
 *  0 = HZA_OK                  success
 */
HZA_API hza_error_t C41_CALL hza_create_module
(
    hza_context_t * hc,
    hza_module_t * * mp
);

/* hza_release_module *******************************************************/
/**
 * Releases the ownership of the given module.
 * Returns:
 *  0 = HZA_OK                  success
 **/
HZA_API hza_error_t C41_CALL hza_release_module
(
    hza_context_t * hc,
    hza_module_t * m
);

/* hza_add_proc *************************************************************/
/**
 * Appends a new proc to the given module.
 * The module must be unbound so that changes can be made.
 * The index of the new proc is hza_module_t.proc_count before the call, or
 * hza_module_t.proc_count - 1 after the call.
 * Returns:
 *  0 = HZA_OK                  success
 **/
HZA_API hza_error_t C41_CALL hza_add_proc
(
    hza_context_t * hc,
    hza_module_t * m
);

/* hza_add_block ************************************************************/
/**
 * Appends a new block to an unbound module.
 * Returns:
 *  0 = HZA_OK                  success
 *  HZAE_ALLOC                  alloc failed
 **/
HZA_API hza_error_t C41_CALL hza_add_block
(
    hza_context_t * hc,
    hza_module_t * m
);

/* hza_add_target ***********************************************************/
/**
 * Appends a new target to an unbound module.
 * A subsequent call to hza_seal_block() will assign this target to some
 * block of the same module.
 * Returns:
 *  0 = HZA_OK                  success
 **/
HZA_API hza_error_t C41_CALL hza_add_target
(
    hza_context_t * hc,
    hza_module_t * m,
    uint32_t block_index
);

/* hza_add_insn *************************************************************/
/**
 * Appends a new instruction to the given module.
 * Returns:
 *  0 = HZA_OK                  success
 */
HZA_API hza_error_t C41_CALL hza_add_insn
(
    hza_context_t * hc,
    hza_module_t * m,
    uint16_t opcode,
    uint16_t a,
    uint16_t b,
    uint16_t c
);

/* hza_seal_block ***********************************************************/
HZA_API hza_error_t C41_CALL hza_seal_block
(
    hza_context_t * hc,
    hza_module_t * m,
    uint32_t exc_target
);

/* hza_seal_proc ************************************************************/
HZA_API hza_error_t C41_CALL hza_seal_proc
(
    hza_context_t * hc,
    hza_module_t * m
);

/* hza_load *****************************************************************/
/**
 * Loads a module. All modules must be "loaded" before they can be imported
 * in tasks.
 * This call verifies that the code is valid and maybe even generate some
 * parallel structure with "optimised" code, or even JIT'ed code.
 * also, the function checks if there is an identical loaded module in which
 * case this one is freed and #mp is set to the module already loaded.
 * Returns:
 *  0 = HZA_OK                  success
 */
HZA_API hza_error_t C41_CALL hza_load
(
    hza_context_t * hc,
    hza_module_t * m,
    hza_module_t * * mp
);

/* hza_import ***************************************************************/
/**
 * Imports a module in a task.
 * Returns:
 *  0 = HZA_OK                  success
 */
HZA_API hza_error_t C41_CALL hza_import
(
    hza_context_t * hc,
    hza_task_t * t,
    hza_module_t * m
);

/* hza_create_task **********************************************************/
/**
 * Creates a new task.
 * The task is created with one reference and put in the suspended queue.
 * Returns:
 *  0 = HZA_OK                  success
 */
HZA_API hza_error_t C41_CALL hza_create_task
(
    hza_context_t * hc,
    hza_task_t * * tp
);

/* hza_release_task *********************************************************/
HZA_API hza_error_t C41_CALL hza_release_task
(
    hza_context_t * hc,
    hza_task_t * t
);

/* hza_kill *****************************************************************/
/**
 * Sends the 'kill' request to the target task.
 * This is not interceptable by the target task.
 * If the ref count is 1 (meaning only this context works with a reference
 * to this task) the the task is destroyed on the spot. Otherwise, the ref
 * count is decremented and whatever other context is the last one with
 * a ref will destroy it.
 **/
HZA_API hza_error_t C41_CALL hza_kill
(
    hza_context_t * hc,
    hza_task_t * t
);

/* hza_enter ****************************************************************/
/**
 * Bla.
 * Returns:
 *  0 = HZA_OK                  success
 **/
HZA_API hza_error_t C41_CALL hza_enter
(
    hza_context_t * hc,
    uint_t module_index,
    uint32_t block_index
);

/* hza_activate *************************************************************/
/**
 * Prepares a task to be executed in the given context.
 * This sets task's runner to current context and removes the task from
 * whatever queue is in and adds it to RUNNING
 * Returns:
 *  0 = HZA_OK                  success
 **/
HZA_API hza_error_t C41_CALL hza_activate
(
    hza_context_t * hc,
    hza_task_t * t
);

/* hza_deactivate ***********************************************************/
/**
 * moves task from running into ready queue.
 **/
HZA_API hza_error_t C41_CALL hza_deactivate
(
    hza_context_t * hc,
    hza_task_t * t
);

/* hza_run ******************************************************************/
/**
 * Runs the active task.
 * Returns:
 *  0 = HZA_OK                  success
 **/
HZA_API hza_error_t C41_CALL hza_run
(
    hza_context_t * hc,
    int insn_count,
    int call_level
);

#endif /* _HZA_H_ */

