#include "libpriv.h"

static int load_module
(
    hzm_t * m
)
{
    hzw_t * w = m->w;
    ssize_t x;

    x = hzmt_append_item(&w->lmt, m);
    if (x < 0)
    {
        w->ma_ec = -x;
        WE(w, "failed appending loaded module (error $i)", -x);
        return HZE_ALLOC;
    }
    /* decrement number of unbound modules */
    w->umn--;

    /* write the module table index into the module */
    m->wmx = x;
    WD(w, "loaded module m$Ui in slot $Ui", m->mid, m->wmx);

    return 0;
}

/* hzm_create ***************************************************************/
HZAPI int C41_CALL hzm_create
(
    hzw_t * w,
    hzm_t * * mp
)
{
    int hze, marc;
    hzm_t * m;

    marc = C41_VAR_ALLOC1Z(&w->mac.ma, *mp);
    if (marc)
    {
        w->ma_ec = marc;
        WE(w, "failed allocating module (error $i)", marc);
        return HZE_ALLOC;
    }

    m = *mp;
    m->w = w;
    m->wmx = -1;
    m->mid = w->mid_seed++;

    hzpv_init(&m->pv, &w->mac.ma, 8);
    hzbv_init(&m->bv, &w->mac.ma, 8);
    c41_u32v_init(&m->tv, &w->mac.ma, 8);
    hziv_init(&m->iv, &w->mac.ma, 8);

    do
    {
        hze = hzm_add_insn(m, HZO__UNWIND_ON_EXCEPTION, 0, 0, 0);
        if (hze) break;
        hze = hzm_add_iblk(m);
        if (hze) break;
        hze = hzm_add_proc(m);
        if (hze) break;

        m->cix = 1;
        m->ctx = 0;
        m->cbx = 1;
        m->sbx = 1;
        m->spx = 1;

        MLOCK(w);
        w->umn++;
        MUNLOCK(w);
    }
    while (0);

    if (hze)
    {
        marc = C41_VAR_FREE1(&w->mac.ma, m);
        if (marc)
        {
            w->ma_ec = marc;
            WF(w, "failed freeing module during creation (error $i)", marc);
            return HZF_FREE;
        }
        return hze;
    }

    return 0;
}

/* free_module **************************************************************/
int C41_CALL free_module
(
    hzm_t * m
)
{
    hzw_t * w = m->w;
    int marc;
    int x;

    WD(w, "freeing module m$Ui...", m->mid);
    if (m->rn)
    {
        WF(w, "BUG: free_module(m$Ui) called while referenced ($Ui)",
           m->mid, m->rn);
        return HZF_BUG;
    }

    if (m->wmx < 0)
    {
        // unbound module
        w->umn--;
    }
    else
    {
        // loaded module with 0 refs
        x = (w->lmt.n -= 1);
        if (m->wmx < x)
        {
            hzm_t * o = w->lmt.a[x];
            o->wmx = m->wmx;
            w->lmt.a[m->wmx] = o;
            WD(w, "relocated m$Ui from slot $Ui to slot $Ui",
               o->mid, x, m->wmx);
        }
    }

    hzpv_free(&m->pv);
    hzbv_free(&m->bv);
    c41_u32v_free(&m->tv);
    hziv_free(&m->iv);
    marc = C41_VAR_FREE1(&w->mac.ma, m);
    if (marc)
    {
        WF(w, "fault freeing module (ma error $i)", marc);
        return HZF_FREE;
    }
    return 0;
}

/* hzm_destroy **************************************************************/
HZAPI int C41_CALL hzm_destroy
(
    hzm_t * m
)
{
    hzw_t * w = m->w;
    int hze;

    MLOCK(w);
    hze = free_module(m);
    MUNLOCK(w);
    return hze;
}

/* hzm_load *****************************************************************/
HZAPI int C41_CALL hzm_load
(
    hzm_t * m
)
{
    hzw_t * w = m->w;
    int hze;
    MLOCK(w);
    hze = load_module(m);
    MUNLOCK(w);
    return hze;
}

/* hzm_add_insn *************************************************************/
HZAPI int C41_CALL hzm_add_insn
(
    hzm_t * m,
    uint16_t opcode,
    uint16_t a,
    uint16_t b,
    uint16_t c
)
{
    hzw_t * w = m->w;
    hzinsn_t * insn;
    insn = hziv_append(&m->iv, 1);
    if (!insn)
    {
        WE(w, "failed appending to insn vector (ma-error $i)", m->iv.ma_rc);
        return HZE_ALLOC;
    }
    insn->opcode = opcode;
    insn->a = a;
    insn->b = b;
    insn->c = c;
    return 0;
}

/* hzm_add_iblk *************************************************************/
HZAPI int C41_CALL hzm_add_iblk
(
    hzm_t * m
)
{
    hzw_t * w = m->w;
    hziblk_t * b;
    b = hzbv_append(&m->bv, 1);
    if (!b)
    {
        WE(w, "failed appending to insn-block vector (ma error $i)",
           m->bv.ma_rc);
        return HZE_ALLOC;
    }
    C41_VAR_ZERO(*b);
    return 0;
}

/* hzm_add_target ***********************************************************/
HZAPI int C41_CALL hzm_add_target
(
    hzm_t * m,
    uint32_t bx
)
{
    hzw_t * w = m->w;
    uint32_t * t;
    t = c41_u32v_append(&m->tv, 1);
    if (!t)
    {
        WE(w, "failed appending to target vector (ma error $i)",
           m->tv.ma_rc);
        return HZE_ALLOC;
    }
    *t = bx;
    return 0;
}

/* hzm_add_proc *************************************************************/
HZAPI int C41_CALL hzm_add_proc
(
    hzm_t * m
)
{
    hzw_t * w = m->w;
    hzproc_t * p;
    p = hzpv_append(&m->pv, 1);
    if (!p)
    {
        WE(w, "failed appending to proc vector (ma error $i)",
           m->pv.ma_rc);
        return HZE_ALLOC;
    }
    C41_VAR_ZERO(*p);
    return 0;
}

/* hzm_seal_iblk ************************************************************/
HZAPI int C41_CALL hzm_seal_iblk
(
    hzm_t * m,
    uint32_t ebx // exception block index
)
{
    hziblk_t * b;

    if (m->sbx == m->bv.n)
    {
        WF(m->w, "no seal block; cannot seal!");
        return HZF_BUG;
    }

    b = &m->bv.a[m->sbx++];
    b->ix = m->cix;
    b->in = m->iv.n - m->cix;
    m->cix = m->iv.n;
    b->tx = m->ctx;
    b->tn = m->tv.n - m->ctx;
    m->ctx = m->tv.n;
    b->et = ebx;

    return 0;
}

/* hzm_seal_proc ************************************************************/
HZAPI int C41_CALL hzm_seal_proc
(
    hzm_t * m
)
{
    hzproc_t * p;
    if (m->spx == m->pv.n)
    {
        WF(m->w, "no seal proc");
        return HZF_BUG;
    }

    p = &m->pv.a[m->spx++];
    p->bx = m->cbx;
    p->bn = m->bv.n - m->cbx;

    return 0;
}

