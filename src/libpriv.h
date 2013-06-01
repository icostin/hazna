#ifndef _HAZNA_V0_ENGINE_H_
#define _HAZNA_V0_ENGINE_H_

#include "../include/hazna.h"

C41_VECTOR_FNS(hztt_t, hzt_t *, hztt);
C41_VECTOR_FNS(hzmt_t, hzm_t *, hzmt);
C41_VECTOR_FNS(hzxcv_t, hzxc_t, hzxcv);
C41_VECTOR_FNS(hziv_t, hzinsn_t, hziv);
C41_VECTOR_FNS(hzbv_t, hziblk_t, hzbv);
C41_VECTOR_FNS(hzpv_t, hzproc_t, hzpv);

#define WLOG(_w, _level, ...) ((void) ( \
        (_w)->log_level >= (_level) \
        && c41_io_fmt((_w)->log, "$c[$s:$s:$i] ", "NFEWID"[(_level)], \
                      __FUNCTION__, __FILE__, __LINE__) >= 0\
        && c41_io_fmt((_w)->log, __VA_ARGS__) >= 0 \
        && c41_io_fmt((_w)->log, "\n") >= 0 \
    ))
#if _DEBUG
#   define WD(_w, ...) WLOG((_w), HZLL_DEBUG, __VA_ARGS__)
#else
#   define WD(...)
#endif

#define WI(_w, ...) WLOG((_w), HZLL_INFO, __VA_ARGS__)
#define WW(_w, ...) WLOG((_w), HZLL_WARN, __VA_ARGS__)
#define WE(_w, ...) WLOG((_w), HZLL_ERROR, __VA_ARGS__)
#define WF(_w, ...) WLOG((_w), HZLL_FATAL, __VA_ARGS__)

#define TLOCK(_hzw) do { hzw_t * _w = (_hzw); \
    int smtrc = c41_smt_mutex_lock(_w->smt, _w->task_mutex); \
    if (!smtrc) break; \
    _w->smt_ec = smtrc; \
    WF(_w, "failed locking task manager mutex ($i)", smtrc); \
    return HZF_TASK_SYNC; } while (0)

#define TUNLOCK(_hzw) do { hzw_t * _w = (_hzw); \
    int smtrc = c41_smt_mutex_unlock(_w->smt, _w->task_mutex); \
    if (!smtrc) break; \
    _w->smt_ec = smtrc; \
    WF(_w, "failed unlocking task manager mutex ($i)", smtrc); \
    return HZF_TASK_SYNC; } while (0)

#define MLOCK(_hzw) do { hzw_t * _w = (_hzw); \
    int smtrc = c41_smt_mutex_lock(_w->smt, _w->module_mutex); \
    if (!smtrc) break; \
    _w->smt_ec = smtrc; \
    WF(_w, "failed locking module manager mutex ($i)", smtrc); \
    return HZF_MODULE_SYNC; } while (0)

#define MUNLOCK(_hzw) do { hzw_t * _w = (_hzw); \
    int smtrc = c41_smt_mutex_unlock(_w->smt, _w->module_mutex); \
    if (!smtrc) break; \
    _w->smt_ec = smtrc; \
    WF(_w, "failed unlocking module manager mutex ($i)", smtrc); \
    return HZF_MODULE_SYNC; } while (0)

int C41_CALL free_module
(
    hzm_t * m
);

#endif /* _HAZNA_V0_ENGINE_H_ */

