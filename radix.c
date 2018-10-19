#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "util.h"
#include "tlog.h"

//#define SLICE 8
#define SLNODES (1<<(SLICE))
#define LEVELS ((63+SLICE)/SLICE)
#define FUNC(x) radixSLICE_##x

#ifdef DEBUG_SPAM
# define dprintf(...) printf(__VA_ARGS__)
#else
# define dprintf(...)
#endif

//#define TRACEMEM

#ifdef TRACEMEM
static int64_t memusage=0;
static int64_t depths=0;
static int64_t gets=0;
#endif

struct node
{
    struct node* nodes[SLNODES];
    uint64_t volatile lock;
    uint64_t volatile delete_lock;
    uint64_t nchildren;
};

#define DELETER      0x0000000100000000
#define ONLY_DELETER 0x0001000000000000
#define ANY_DELETERS 0xffffffff00000000
#define ANY_WRITERS  0x00000000ffffffff

#define TOP_EMPTY 0xffffffffffffffff

struct node *FUNC(new)(void)
{
#ifdef TRACEMEM
    memusage=1;
    depths=gets=0;
#endif
    return Zalloc(sizeof(struct node));
}

static inline uint32_t sl(uint64_t key, int lev)
{
    return key>>(lev*SLICE) & (SLNODES-1);
}

static __attribute__((unused)) void display(struct node *restrict n, int lev)
{
    for (int k=lev; k<LEVELS; k++)
        printf(" ");
    printf("nchildren=%lu, lock=%lx\n", n->nchildren, n->lock);
    for (uint32_t i=0; i<SLNODES; i++)
        if (n->nodes[i])
        {
            for (int k=lev; k<LEVELS; k++)
                printf(" ");
            printf("• %02x:\n", i);
            if (lev)
                display(n->nodes[i], lev-1);
        }
}

static void teardown(struct node *restrict n, int lev)
{
    if (lev)
    {
        for (uint32_t i=0; i<SLNODES; i++)
            if (n->nodes[i])
                teardown(n->nodes[i], lev-1);
    }
    Free(n);
#ifdef TRACEMEM
    util_fetch_and_sub64(&memusage, 1);
#endif
}

void FUNC(delete)(struct node *restrict n)
{
    teardown(n, LEVELS-1);
#ifdef TRACEMEM
    if (memusage)
        fprintf(stderr, "==== memory leak: %ld left ====\n", memusage), abort();
#endif
}

static int insert(struct node *restrict n, int lev, uint64_t key, void *value)
{
    uint32_t slice = sl(key, lev);
    dprintf("-> %d: %02x\n", lev, slice);

    if (!lev)
    {
        if (util_bool_compare_and_swap64(&n->nodes[slice], 0, value))
            util_fetch_and_add64(&n->nchildren, 1);
        else
            n->nodes[slice] = value; /* update; we know there are no deleters */
        return 0;
    }

    while (util_fetch_and_add64(&n->lock, 1) & ANY_DELETERS)
    {
        util_fetch_and_sub64(&n->lock, 1); /* no atomic_sub? */
        sleep(0);
    }

retry:;
    struct node *restrict m;
    if ((m = n->nodes[slice]))
    {
        int ret = insert(m, lev-1, key, value);
        util_fetch_and_sub64(&n->lock, 1);
        return ret;
    }

    dprintf("new alloc\n");
#ifdef TRACEMEM
    util_fetch_and_add64(&memusage, 1);
#endif
    m = Zalloc(sizeof(struct node));
    if (!m)
        return ENOMEM;

    int ret = insert(m, lev-1, key, value);
    if (ret)
    {
        Free(m);
        util_fetch_and_sub64(&n->lock, 1);
        return ret;
    }

    if (util_bool_compare_and_swap64(&n->nodes[slice], 0, m))
    {
        util_fetch_and_add64(&n->nchildren, 1);
        util_fetch_and_sub64(&n->lock, 1);
        return 0;
    }
    /* Someone else just created this subtree for us. */
    teardown(m, lev-1);
    goto retry;
}

int FUNC(insert)(struct node *restrict n, uint64_t key, void *value)
{
    dprintf("insert(%016lx)\n", key);

    /* value of 0 is indistinguishable from "not existent" */
    if (!value)
        return 0;

    int ret = insert(n, LEVELS-1, key, value);
    if (ret)
        return ret;

    //display(n, LEVELS-1);
    return 0;
}

/* return 1 if we removed last subtree, making n empty */
static int nremove(struct node *restrict n, int lev, uint64_t key, void**restrict value)
{
    uint32_t slice = sl(key, lev);
    dprintf("-> %d: %02x\n", lev, slice);
    if (!lev)
    {
        if (n->nodes[slice])
        {
            *value = n->nodes[slice];
            n->nodes[slice] = 0;
            return util_fetch_and_sub64(&n->nchildren, 1) == 1;
        }
        else
            return 0;
    }

    while (util_fetch_and_add64(&n->lock, 1) & ANY_DELETERS)
    {
        util_fetch_and_sub64(&n->lock, 1);
        sleep(0);
    }

    struct node *m = n->nodes[slice];
    if (!m || !nremove(m, lev-1, key, value))
    {
        util_fetch_and_sub64(&n->lock, 1);
        return 0;
    }

    util_fetch_and_add64(&n->lock, DELETER-1); /* convert write lock to delete lock */
    while (n->lock & ANY_WRITERS)
        dprintf("LOCK: %lx\n", n->lock)
        ; /* wait for writers to go away */

    /* There still may be other _deleters_, for different keys in this
     * subtree (in convoluted cases even of the same child, despite us
     * having been told it's empty!).
     */

    do; while (util_fetch_and_or64(&n->lock, ONLY_DELETER) & ONLY_DELETER);

    if (!n->nodes[slice] || m->nchildren) /* we had no lock -- a writer could have created something */
    {
        util_fetch_and_sub64(&n->lock, DELETER|ONLY_DELETER);
        return 0;
    }

    n->nodes[slice] = 0;
    int was_only = !--n->nchildren;
    util_fetch_and_sub64(&n->lock, DELETER|ONLY_DELETER);
#ifdef TRACEMEM
    util_fetch_and_sub64(&memusage, 1);
#endif
    Free(m);
    return was_only;
}

void *FUNC(remove)(struct node *restrict n, uint64_t key)
{
    dprintf("remove(%016lx)\n", key);
    void* value = 0;
    nremove(n, LEVELS-1, key, &value);
    //display(n, LEVELS-1);
    return value;
}

#ifdef TRACEMEM
# define INCDEPTHS util_fetch_and_add64(&depths, 1)
#else
# define INCDEPTHS do;while(0)
#endif

#define GETL(l) \
    if ((l)*SLICE < 64)			\
    {					\
        INCDEPTHS;			\
        n = n->nodes[sl(key, (l))];	\
        if (!n)				\
            return 0;			\
    }

void* FUNC(get)(struct node *restrict n, uint64_t key)
{
#ifdef TRACEMEM
    util_fetch_and_add64(&gets, 1);
#endif
    dprintf("get(%016lx)\n", key);
    // for (int lev = LEVELS-1; lev>=0; lev--)
    GETL(15);
    GETL(14);
    GETL(13);
    GETL(12);
    GETL(11);
    GETL(10);
    GETL(9);
    GETL(8);
    GETL(7);
    GETL(6);
    GETL(5);
    GETL(4);
    GETL(3);
    GETL(2);
    GETL(1);
    GETL(0);
    return n;
}

size_t FUNC(get_size)(struct node *restrict n)
{
#ifdef TRACEMEM
    return memusage*sizeof(struct node);
#else
    return 0;
#endif
}

void FUNC(get_stats)(void *c, uint64_t *buf, int nstat)
{
#ifdef TRACEMEM
    if (nstat>=1)
        buf[0]=memusage;
    if (nstat>=2)
        buf[1]=depths;
    if (nstat>=3)
        buf[2]=gets;
#endif
}

uint64_t FUNC(debug)(struct node *restrict n, uint64_t arg)
{
    return 0;
}
