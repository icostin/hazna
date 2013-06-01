#ifndef _HAZNA_V0_H_
#define _HAZNA_V0_H_

#include <c41.h>

#if HZ_STATIC
# define HZAPI
#elif HZ_DL_BUILD
# define HZAPI C41_DL_EXPORT
#else
# define HZAPI C41_DL_IMPORT
#endif

#define HZMH_SIG0 ((uint8_t const *) "[hazna]\n")
#define HZMH_SIG0_LEN 8

#define HZ_PAGE_SHIFT 12
#define HZ_PAGE_SIZE (1 << HZ_PAGE_SHIFT) // 4KB

enum hze_enum
{
    HZ_OK = 0,
    HZF_NO_CODE, // not implemented
    HZF_BUG, // used for 'soft' assertions
    HZF_TASK_SYNC, // failed locking/unlocking task mutex
    HZF_MODULE_SYNC, // failed locking/unlocking module mutex
    HZF_FREE, // freeing memory should not fail if correct args are passed and
    // there is no heap corruption; otherwise it can fail
    HZF_REF,
    HZF__LIMIT, // separator for fatal errors and recoverable errors
    HZE_ALLOC,
    HZE_LEAK,
    HZE_PM_FINISH,
    HZE_TASK_MUTEX_CREATE,
    HZE_MODULE_MUTEX_CREATE,
};

#define HZLL_DEBUG 5
#define HZLL_INFO 4
#define HZLL_WARN 3
#define HZLL_ERROR 2
#define HZLL_FATAL 1
#define HZLL_NONE 0

/* opcodes */
enum hzo_enum // {{{
{
    HZO_CONST_1 = 0,
    HZO_CONST_2,
    HZO_CONST_4,
    HZO_CONST_8,
    HZO_CONST_16,
    HZO_CONST_32,
    HZO_CONST_64,
    HZO_CONST_128,

    HZO_LOAD_1_A32,
    HZO_LOAD_2_A32,
    HZO_LOAD_4_A32,
    HZO_LOAD_8_A32,
    HZO_LOAD_16_A32,
    HZO_LOAD_32_A32,
    HZO_LOAD_64_A32,
    HZO_LOAD_128_A32,

    HZO_SAVE_1_A32,
    HZO_SAVE_2_A32,
    HZO_SAVE_4_A32,
    HZO_SAVE_8_A32,
    HZO_SAVE_16_A32,
    HZO_SAVE_32_A32,
    HZO_SAVE_64_A32,
    HZO_SAVE_128_A32,

    HZO_LOAD_1_A64,
    HZO_LOAD_2_A64,
    HZO_LOAD_4_A64,
    HZO_LOAD_8_A64,
    HZO_LOAD_16_A64,
    HZO_LOAD_32_A64,
    HZO_LOAD_64_A64,
    HZO_LOAD_128_A64,

    HZO_SAVE_1_A64,
    HZO_SAVE_2_A64,
    HZO_SAVE_4_A64,
    HZO_SAVE_8_A64,
    HZO_SAVE_16_A64,
    HZO_SAVE_32_A64,
    HZO_SAVE_64_A64,
    HZO_SAVE_128_A64,

    /* multi-load dest src count */
    HZO_MLOAD_1_A32,
    HZO_MLOAD_2_A32,
    HZO_MLOAD_4_A32,
    HZO_MLOAD_8_A32,
    HZO_MLOAD_16_A32,
    HZO_MLOAD_32_A32,
    HZO_MLOAD_64_A32,
    HZO_MLOAD_128_A32,

    HZO_MSAVE_1_A32,
    HZO_MSAVE_2_A32,
    HZO_MSAVE_4_A32,
    HZO_MSAVE_8_A32,
    HZO_MSAVE_16_A32,
    HZO_MSAVE_32_A32,
    HZO_MSAVE_64_A32,
    HZO_MSAVE_128_A32,

    HZO_MLOAD_1_A64,
    HZO_MLOAD_2_A64,
    HZO_MLOAD_4_A64,
    HZO_MLOAD_8_A64,
    HZO_MLOAD_16_A64,
    HZO_MLOAD_32_A64,
    HZO_MLOAD_64_A64,
    HZO_MLOAD_128_A64,

    HZO_MSAVE_1_A64,
    HZO_MSAVE_2_A64,
    HZO_MSAVE_4_A64,
    HZO_MSAVE_8_A64,
    HZO_MSAVE_16_A64,
    HZO_MSAVE_32_A64,
    HZO_MSAVE_64_A64,
    HZO_MSAVE_128_A64,

    HZO_BRANCH_1, // sets the target block using the given 1-bit register
    HZO_BRANCH_2,
    HZO_BRANCH_4,
    HZO_BRANCH_8,
    HZO_BRANCH_16,
    HZO_BRANCH_32, 
    HZO_BRANCH_64, 
    HZO_BRANCH_128, 

    HZO__UNWIND_ON_EXCEPTION,   // this opcode is only allowed in the 'ghost'
                                // block with index 0
    HZO_CALL, // direct call
    HZO_CALL_P32, // indirect call; C32 is some opaque proc pointer

    HZO_COPY_1,
    HZO_COPY_2,
    HZO_COPY_4,
    HZO_COPY_8,
    HZO_COPY_16,
    HZO_COPY_32,
    HZO_COPY_64,
    HZO_COPY_128,

    HZO_ZX_2_1,
    HZO_ZX_4_1,
    HZO_ZX_4_2,
    HZO_ZX_8_1,
    HZO_ZX_8_2,
    HZO_ZX_8_4,
    HZO_ZX_16_1,
    HZO_ZX_16_2,
    HZO_ZX_16_4,
    HZO_ZX_16_8,
    HZO_ZX_32_1,
    HZO_ZX_32_2,
    HZO_ZX_32_4,
    HZO_ZX_32_8,
    HZO_ZX_32_16,
    HZO_ZX_64_1,
    HZO_ZX_64_2,
    HZO_ZX_64_4,
    HZO_ZX_64_8,
    HZO_ZX_64_16,
    HZO_ZX_64_32,
    HZO_ZX_128_1,
    HZO_ZX_128_2,
    HZO_ZX_128_4,
    HZO_ZX_128_8,
    HZO_ZX_128_16,
    HZO_ZX_128_32,
    HZO_ZX_128_64,

    HZO_SX_2_1,
    HZO_SX_4_1,
    HZO_SX_4_2,
    HZO_SX_8_1,
    HZO_SX_8_2,
    HZO_SX_8_4,
    HZO_SX_16_1,
    HZO_SX_16_2,
    HZO_SX_16_4,
    HZO_SX_16_8,
    HZO_SX_32_1,
    HZO_SX_32_2,
    HZO_SX_32_4,
    HZO_SX_32_8,
    HZO_SX_32_16,
    HZO_SX_64_1,
    HZO_SX_64_2,
    HZO_SX_64_4,
    HZO_SX_64_8,
    HZO_SX_64_16,
    HZO_SX_64_32,
    HZO_SX_128_1,
    HZO_SX_128_2,
    HZO_SX_128_4,
    HZO_SX_128_8,
    HZO_SX_128_16,
    HZO_SX_128_32,
    HZO_SX_128_64,

    /* not */
    HZO_NOT_1,
    HZO_NOT_2,
    HZO_NOT_4,
    HZO_NOT_8,
    HZO_NOT_16,
    HZO_NOT_32,
    HZO_NOT_64,
    HZO_NOT_128,

    /* add modulo|unsigned-overflow-detection|signed-overflow-detection|.... */
    HZO_ADDM_1, // add modulo 2^N
    HZO_ADDUO_1, // add unsigned with overflow detection
    HZO_ADDSO_1, // add signed with overflow detection
    HZO_ADDUD_1, // add unsigned storing in a double-sized destination
    HZO_ADDSD_1, // add signed storing in a double-sized destination
};
// end of opcode list }}}

typedef struct hzt_s                            hzt_t;
typedef struct hzw_s                            hzw_t;
typedef struct hzxc_s                           hzxc_t;
typedef struct hzinsn_s                         hzinsn_t; // instruction
typedef struct hziblk_s                         hziblk_t; // insn block
typedef struct hzproc_s                         hzproc_t;
typedef struct hzm_s                            hzm_t; // code module

C41_VECTOR_DECL(hztt_t, hzt_t *); // task table
C41_VECTOR_DECL(hzxcv_t, hzxc_t);
C41_VECTOR_DECL(hziv_t, hzinsn_t);
C41_VECTOR_DECL(hzbv_t, hziblk_t);
C41_VECTOR_DECL(hzpv_t, hzproc_t);
C41_VECTOR_DECL(hzmt_t, hzm_t *); // module table

struct hzxc_s // execution context
{
    uint32_t mx; // module index - index is relative to task's
    uint32_t bx;
    uint32_t ix;
    uint32_t tgt;
};

struct hzt_s
{
    uint8_t * r; // current register ptr
    hzw_t * w; // world
    c41_u8v_t rv; // register space vector
    hzxcv_t xcv; // execution context vector
    hzmt_t mt; // module table
    uint_t tid; // task id - unique id among tasks throughout the life of
                // the world
    uint_t wtx; // world task index - work with this with world->task_mutex locked!
};

struct hzw_s
{
    hztt_t tt; // task table
    hzmt_t lmt; // loaded module table
    c41_esm_t pm; // page manager
    c41_smt_t * smt; // multithreading interface
    c41_smt_mutex_t * task_mutex; // task manager mutex
    c41_smt_mutex_t * module_mutex; // module manager mutex

    uint_t tid_seed;
    uint_t mid_seed;
    uint_t umn; // number of unbound modules (modules created but not loaded)

    c41_io_t * log;
    uint_t log_level;
    uint_t ma_ec; // error code from mem allocator
    uint_t smt_ec; // error code from multithreading funcs
    c41_ma_counter_t mac;
};

struct hzinsn_s
{
    uint16_t opcode;
    uint16_t a, b, c;
};

struct hziblk_s
{
    uint32_t ix; // insn index
    uint32_t in; // insn num
    uint32_t tx; // target index
    uint32_t tn; // target num
    uint32_t et; // exception target - 0 for 'exception not handled'
    uint32_t px; // proc index
};

struct hzproc_s
{
    uint32_t bx; // index of first block (also the entry point of the proc)
    uint32_t bn; // number of blocks
    uint32_t rn; // number of bytes in register-space
};

struct hzm_s
{
    hzpv_t pv; // proc vector
    hzbv_t bv; // block vector
    c41_u32v_t tv; // target vector
    hziv_t iv; // insn vector
    // c41_u64v_t c64v; // 64-bit const vector
    uint32_t cix; // current insn index
    uint32_t cbx; // current iblk index
    uint32_t ctx; // current target index
    uint32_t cpx; // current proc index
    int rn; // number of refs
    int wmx; // index in loaded|unbound module table
    int mid;
    hzw_t * w;
};

/* hzlib_name ***************************************************************/
HZAPI char const * C41_CALL hzlib_name ();

/* hzw_init *****************************************************************
 * inits world.
 * returns 0 on success, HZE_xxx on error
 */
HZAPI int C41_CALL hzw_init 
(
    hzw_t * w,
    c41_ma_t * ma, // memory allocator
    c41_smt_t * smt, // multithreading interface
    c41_io_t * log, // this can be NULL if log_level is HZLL_NONE
    uint_t log_level
);

/* hzw_finish ***************************************************************/
/* Sends the four horsemen!
 * Returns:
 *  0               another job well done
 *  HZF_BUG         unbound modules present, tasks running or other silly bugs
 */
HZAPI int C41_CALL hzw_finish
(
    hzw_t * w
);

/* hzw_log_init *************************************************************/
HZAPI void C41_CALL hzw_log_init
(
    hzw_t * w,
    c41_io_t * log,
    uint_t level
);

/* hzt_create ***************************************************************
 * Creates a task inside the given world.
 */
HZAPI int C41_CALL hzt_create
(
    hzw_t * w,
    hzt_t * * tp
);

/* hzt_destroy **************************************************************/
HZAPI int C41_CALL hzt_destroy
(
    hzt_t * t
);

/* hzt_import ***************************************************************/
/**
 * Imports a module in a task.
 * Returns:
 *  0                   success
 */
HZAPI int C41_CALL hzt_import
(
    hzt_t * t,
    hzm_t * m
);

/* hzm_create ***************************************************************/
/**
 * Creates an empty module.
 * The module is unbound which means changes can be made to its code.
 * Its ref count stays 0 when unbound.
 * When all desired code is inserted in the module, use hzm_load() to bind
 * the module to the world.
 * If the module is not loaded, hzm_destroy() must be called 
 * before the apocalypse hzw_finish()
 *
 * Returns:
 *  0               for success
 *  HZE_ALLOC       no mem
 */
HZAPI int C41_CALL hzm_create
(
    hzw_t * w,
    hzm_t * * mp
);

/* hzm_destroy **************************************************************/
HZAPI int C41_CALL hzm_destroy
(
    hzm_t * m
);

/* hzm_add_proc *************************************************************/
/**
 * Adds an empty procedure.
 * The index of the procedure is the value of m->pv.n before the call, or
 * m->pv.n - 1 after the call.
 */
HZAPI int C41_CALL hzm_add_proc
(
    hzm_t * m
);

/* hzm_add_iblk *************************************************************/
/**
 * Adds an empty instruction block.
 * The index of the block is the value of m->bv.n before the call, or
 * m->bv.n - 1 after the call.
 */
HZAPI int C41_CALL hzm_add_iblk
(
    hzm_t * m
);

/* hzm_add_target ***********************************************************/
/**
 * Adds a block target.
 * The index of the block is the value of m->tv.n before the call, or
 * m->tv.n - 1 after the call.
 */
HZAPI int C41_CALL hzm_add_target
(
    hzm_t * m,
    uint32_t bx
);

/* hzm_add_insn *************************************************************/
/**
 * Adds an instruction.
 * The index of the instruction is the value of m->iv.n before the call, 
 * or m->iv - 1 after the call.
 * Returns:
 *  0               success
 *  HZE_ALLOC       no mem
 */
HZAPI int C41_CALL hzm_add_insn
(
    hzm_t * m,
    uint16_t opcode,
    uint16_t a,
    uint16_t b,
    uint16_t c
);

/* hzm_seal_iblk ************************************************************/
/**
 * Seals the current instruction block.
 * This function modifies the block with index m->cbx and sets its instructions
 * to those from index m->cix up to the last instruction (m->iv.n - 1).
 * Same thing happens to code blocks with indexes m->cbx...(m->bv.n - 1).
 * Then the indexes of current instructions and targets are set to the size of
 * those vectors and the current block index is incremented.
 * Block index 0 is a 'ghost' block that contains 1 instruction which tells
 * the interpreter to unwind the stack on exception so ebx = 0 means there is
 * no exception handling local in that block.
 */
HZAPI int C41_CALL hzm_seal_iblk
(
    hzm_t * m,
    uint32_t ebx // exception block index
);

/* hzm_seal_proc ************************************************************/
/**
 * Seals the current procedure.
 * This function modifies the procedure with inde m->cpx and sets 
 */
HZAPI int C41_CALL hzm_seal_proc
(
    hzm_t * m
);

/* hzm_load *****************************************************************/
/**
 * Takes an unbound module and moves it to the loaded module list.
 * Once the module is loaded its code should not be modified.
 * Returns:
 *  0               success
 */
HZAPI int C41_CALL hzm_load
(
    hzm_t * m
);

/* hz_is_fatal **************************************************************/
C41_INLINE int hz_is_fatal (int hze)
{
    return hze && hze < HZF__LIMIT;
}

#endif /* _HAZNA_V0_H_ */

