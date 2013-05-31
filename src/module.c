#include "libpriv.h"

/* attach_unbound ***********************************************************/
static int attach_unbound
(
    hzm_t * m
)
{
    hzw_t * w = m->w;
    ssize_t x;
    x = hzmt_append_item(&w->umt, m);
    if (x < 0)
    {
        w->ma_ec = -x;
        WE(w, "failed appending unbound module (error $i)", -x);
        return HZE_ALLOC;
    }
    m->wmx = x;
    return 0;
}

/* unbound_to_loaded ********************************************************/
static int unbound_to_loaded
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
    w->umt.n -= 1;
    /* it the module moved to loaded is not the last, then move the last
     * module at its index */
    if (m->wmx < w->umt.n)
    {
        hzm_t * o = w->umt.a[w->umt.n];
        w->umt.a[m->wmx] = o;
        o->wmx = m->wmx;
    }

    m->wmx = x;

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

    hzpv_init(&m->pv, &w->mac.ma, 8);
    hzbv_init(&m->bv, &w->mac.ma, 8);
    c41_u32v_init(&m->tv, &w->mac.ma, 8);
    hziv_init(&m->iv, &w->mac.ma, 8);

    do
    {
        MLOCK(w);
        hze = attach_unbound(m);
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

/* hzm_load *****************************************************************/
HZAPI int C41_CALL hzm_load
(
    hzm_t * m
)
{
    hzw_t * w = m->w;
    int hze;
    MLOCK(w);
    hze = unbound_to_loaded(m);
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

