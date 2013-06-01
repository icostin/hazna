#include "libpriv.h"

/* task_create **************************************************************/
static int C41_CALL task_create
(
    hzw_t * w,
    hzt_t * * tp
)
{
    hzt_t * * p;
    hzt_t * t;
    int marc;

    /* extend the task table to hold a new task pointer */
    p = hztt_append(&w->tt, 1);
    if (!p)
    {
        WE(w, "failed extending task table");
        return HZE_ALLOC;
    }

    /* allocate the new task */
    marc = C41_VAR_ALLOC1Z(&w->mac.ma, *p);
    if (marc)
    {
        WE(w, "failed allocating memory for a new task (ma error: $i)", marc);
        w->tt.n -= 1;
        return HZE_ALLOC;
    }
    *tp = t = *p;

    c41_u8v_init(&t->rv, &w->mac.ma, 24);

    t->w = w;
    t->wtx = w->tt.n - 1;
    t->tid = w->tid_seed++;
    WD(w, "created task t$Ui and stored it in slot $Ui!", t->tid, t->wtx);
    return 0;
}

/* task_destroy *************************************************************/
static int C41_CALL task_destroy
(
    hzt_t * t
)
{
    hzw_t * w = t->w;
    uint_t marc, i, n, tid = t->tid;
    marc = c41_u8v_free(&t->rv);
    if (marc)
    {
        WF(w, "failed freeing memory for regs tid=$u (ma error: $i)", tid, 
           marc);
        return HZF_FREE;
    }
    i = t->wtx;
    n = w->tt.n -= 1;
    marc = C41_VAR_FREE1(&w->mac.ma, t);
    if (marc)
    {
        WF(w, "failed freeing memory for task tid=$i (ma error: $i)", 
           tid, marc);
        return HZF_FREE;
    }
    if (i != n)
    {
        w->tt.a[i] = t = w->tt.a[n];
        t->wtx = i;
        WD(w, "destroyed task t$Ui (slot $Ui) and moved in its slot "
           "task t$Ui (from slot $Ui)", tid, i, t->tid, n);
    }
    else
    {
        WD(w, "destroyed task t$Ui from last slot $Ui", tid, i);
    }
    WD(w, "---");

    return 0;
}

/* hzt_create ***************************************************************/
HZAPI int C41_CALL hzt_create
(
    hzw_t * w,
    hzt_t * * tp
)
{
    int hzrc;
    TLOCK(w);
    hzrc = task_create(w, tp);
    TUNLOCK(w);
    return hzrc;
}

/* hzt_destroy **************************************************************/
HZAPI int C41_CALL hzt_destroy
(
    hzt_t * t
)
{
    int hzrc;
    hzw_t * w = t->w;
    TLOCK(w);
    hzrc = task_destroy(t);
    WD(w, "task destroy: $i", hzrc);
    TUNLOCK(w);
    return hzrc;
}

