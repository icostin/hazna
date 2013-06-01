#include "libpriv.h"

/* hzw_init *****************************************************************/
HZAPI int C41_CALL hzw_init
(
    hzw_t * w,
    c41_ma_t * ma,
    c41_smt_t * smt,
    c41_io_t * log,
    uint_t log_level
)
{
    uint_t marc;
    uint_t smtrc;
    int rc;

    do
    {
        /* start with a clean world */
        C41_VAR_ZERO(*w);

        /* wrap the allocator through a counting allocator 
         * we might want to do this only when the user asks us to (through some
         * flag) but in the mean time it doesn't hurt.
         */
        ma = c41_ma_counter_init(&w->mac, ma, 
                                 C41_SSIZE_MAX, C41_SSIZE_MAX, C41_SSIZE_MAX);

        /* store the multithreading interface */
        w->smt = smt;

        /* init logging */
        hzw_log_init(w, log, log_level);

        /* init the task management mutex */
        smtrc = c41_smt_mutex_create(&w->task_mutex, smt, ma);
        if (smtrc)
        {
            w->smt_ec = smtrc;
            WE(w, "task management mutex creation failed: $Ui", smtrc);
            rc = HZE_TASK_MUTEX_CREATE;
            break;
        }

        /* init the module management mutex */
        smtrc = c41_smt_mutex_create(&w->module_mutex, smt, ma);
        if (smtrc)
        {
            w->smt_ec = smtrc;
            WE(w, "task management mutex creation failed: $Ui", smtrc);
            rc = HZE_TASK_MUTEX_CREATE;
            break;
        }

        /* init page manager:
         * max data is 4KB * 0x7FFFFFFE => 0x7FF_FFFF_E000 (almost 8 TB)
         */
        c41_esm_init(&w->pm, ma, HZ_PAGE_SIZE, 0x7FFFFFFE, 16, 0x10000);

        /* init task table */
        hztt_init(&w->tt, ma, 6);
        /* preallocate some entries for tasks */
        marc = hztt_extend(&w->tt, 4);
        if (marc)
        {
            w->ma_ec = marc;
            WE(w, "task-table extend failed: $Ui", marc);
            rc = HZE_ALLOC;
            break;
        }

        /* init loaded module table */
        hzmt_init(&w->lmt, ma, 6);

        /* all done! */
        rc = 0;
    }
    while (0);

    if (rc) hzw_finish(w);

    WD(w, "init: $i", rc);
    return rc;
}

/* hzw_finish ***************************************************************/
HZAPI int C41_CALL hzw_finish
(
    hzw_t * w
)
{
    int hze;
    uint_t marc;
    uint_t smtrc;
    uint_t esmrc;
    c41_ma_t * ma = &w->mac.ma;
    uint_t i;

    WD(w, "finishing...");

    if (w->umn)
    {
        WF(w, "BUG: unbound modules ($i) still exist at world's end", w->umn);
        return HZF_BUG;
    }

    if (w->task_mutex)
    {
        smtrc = c41_smt_mutex_destroy(w->task_mutex, w->smt, ma);
        if (smtrc)
        {
            WE(w, "failed destroying task manager mutex: $Ui", smtrc);
        }
    }
    if (w->module_mutex)
    {
        smtrc = c41_smt_mutex_destroy(w->module_mutex, w->smt, ma);
        if (smtrc)
        {
            WE(w, "failed destroying module manager mutex: $Ui", smtrc);
        }
    }

    for (i = 0; i < w->lmt.n; ++i)
    {
        hze = free_module(w->lmt.a[i]);
        if (hz_is_fatal(hze)) return hze;
    }

    marc = hzmt_free(&w->lmt);
    if (marc)
    {
        WE(w, "hzmt_free failed: $i", marc);
        return HZF_FREE;
    }

    marc = hztt_free(&w->tt);
    if (marc)
    {
        WE(w, "hztt_free failed: $i", marc);
        return HZF_FREE;
    }

    esmrc = c41_esm_finish(&w->pm);
    if (esmrc)
    {
        WE(w, "error finishing page manager: $i", esmrc);
        return HZE_PM_FINISH;
    }

    if (w->mac.total_size)
    {
        WE(w, "memory leak: size=$z count=$z", 
           w->mac.total_size, w->mac.count);
    }

    return 0;
}


/* hzw_log_init *************************************************************/
HZAPI void C41_CALL hzw_log_init
(
    hzw_t * w,
    c41_io_t * log,
    uint_t level
)
{
    w->log = log;
    w->log_level = level;
}

