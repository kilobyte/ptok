#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "util.h"
#include "tlog.h"

#define SLICE 4
#define SLNODES (1<<(SLICE))
#define LEVELS ((63+SLICE)/SLICE)
#define FUNC(x) tcradix_##x

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

struct tcrnode
{
    struct tcrnode* nodes[SLNODES];
    uint64_t only_key;
    void*    only_val;
    uint64_t nchildren;
};

struct tcrhead
{
    struct tcrnode root;
    uint64_t volatile write_status;
    uint64_t pad[4]; // TODO: is avoiding cacheline dirtying worth it?
    pthread_mutex_t mutex;
    struct tcrnode *deleted_node;
};

#define TOP_EMPTY 0xffffffffffffffff

struct tcrhead *FUNC(new)(void)
{
#ifdef TRACEMEM
    memusage=1;
    depths=gets=0;
#endif
    struct tcrhead *n = Zalloc(sizeof(struct tcrhead));
    if (!n)
        return 0;
    n->root.only_key = TOP_EMPTY;
    pthread_mutex_init(&n->mutex, 0);
    return n;
}

static inline void write_poke(struct tcrhead *restrict h)
{
    util_fetch_and_add64(&h->write_status, 1);
}

static inline uint32_t sl(uint64_t key, int lev)
{
    return key>>(lev*SLICE) & (SLNODES-1);
}

static __attribute__((unused)) void display(struct tcrnode *restrict n, int lev)
{
    for (int k=lev; k<LEVELS; k++)
        printf(" ");
    printf("%sonly_key=%016lx, only_val=%016lx%s, nchildren=%lu\n",
        lev?"":"{", n->only_key, (uint64_t)n->only_val, lev?"":"}", n->nchildren);
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

static void teardown(struct tcrnode *restrict n, int lev)
{
    if (lev)
    {
        if (n->only_key)
        {
            struct tcrnode *restrict m = n->nodes[sl(n->only_key, lev)];
            if (m)
                teardown(m, lev-1);
        }
        else
            for (uint32_t i=0; i<SLNODES; i++)
                if (n->nodes[i])
                    teardown(n->nodes[i], lev-1);
    }
    Free(n);
#ifdef TRACEMEM
    memusage--;
#endif
}

void FUNC(delete)(struct tcrhead *restrict n)
{
    pthread_mutex_destroy(&n->mutex);
    for (struct tcrnode *m = n->deleted_node; m; )
    {
        struct tcrnode *mm=m->nodes[0];
        Free(m);
        m=mm;
    }
    teardown(&n->root, LEVELS-1);
#ifdef TRACEMEM
    if (memusage)
        fprintf(stderr, "==== memory leak: %ld left ====\n", memusage), abort();
#endif
}


static struct tcrnode *alloc_node(struct tcrhead *c)
{
    if (!c->deleted_node)
        return Zalloc(sizeof(struct tcrnode));
    struct tcrnode *n = c->deleted_node;
    c->deleted_node = n->nodes[0];
    n->nodes[0] = 0;

#ifdef DEBUG_SPAM
    for (size_t i=0; i<sizeof(*n); i++)
        if (((const char*)(n))[i])
            fprintf(stderr, "reclaimed node not clean at byte %zx\n", i);
#endif

    return n;
}

static int insert(struct tcrhead *restrict c, struct tcrnode *restrict n,
                  int lev, uint64_t key, void *value);

static inline int insert_child(struct tcrhead *restrict c,
                               struct tcrnode *restrict n,
                               int lev, uint64_t key, void *value)
{
    uint32_t slice = sl(key, lev);
    struct tcrnode *restrict m;
    if ((m = n->nodes[slice]))
        return insert(c, m, lev-1, key, value);

    dprintf("new alloc\n");
    m = alloc_node(c);
#ifdef TRACEMEM
    memusage++;
#endif
    if (!m)
        return ENOMEM;

    m->only_key = key;
    m->only_val = value;
    m->nchildren = (lev==1);
    if (lev==1)
        m->nodes[sl(key, 0)] = value;
    else if (!key) // nasty special case: key of 0
    {
        dprintf("inserting 0 @%d\n", lev);
        int ret = insert(c, m, lev-1, key, value);
        if (ret)
        {
            teardown(m, lev-1);
            return ret;
        }
    }
    n->nodes[slice] = m;
    n->nchildren++;
    return 0;
}

static int insert(struct tcrhead *restrict c, struct tcrnode *restrict n,
                  int lev, uint64_t key, void *value)
{
    dprintf("-> %d: %02x\n", lev, sl(key, lev));

    if (!lev)
    {
        uint32_t slice = sl(key, lev);
        if (!n->nodes[slice])
            n->nchildren++;
        n->nodes[slice] = value;
        return 0;
    }

    if (n->only_key)
    {
        if (n->only_key == key)
            n->only_val = value;
        else
        {
            // need to materialize the has-been-only node
            int ret = insert_child(c, n, lev, n->only_key, n->only_val);
            if (ret)
                return ret;
            n->only_key = 0;
        }
    }

    return insert_child(c, n, lev, key, value);
}

int FUNC(insert)(struct tcrhead *restrict n, uint64_t key, void *value)
{
    dprintf("insert(%016lx)\n", key);

    /* value of 0 is indistinguishable from "not existent" */
    if (!value)
        return 0;

    pthread_mutex_lock(&n->mutex);
    write_poke(n);
    if (n->root.only_key == TOP_EMPTY && !n->root.nchildren)
    {
        n->root.only_val = value;
        n->root.only_key = key;
    }

    int ret = insert(n, &n->root, LEVELS-1, key, value);
    write_poke(n);
    pthread_mutex_unlock(&n->mutex);
    if (ret)
        return ret;

    //display(&n->root, LEVELS-1);
    return 0;
}

/* return 1 if we removed last subtree, making n empty */
static int nremove(struct tcrhead *restrict c, struct tcrnode *restrict n,
                   int lev, uint64_t key, void**restrict value)
{
    if (n->only_key == key && key)
    {
        n->only_key = 0;
        *value = n->only_val;
    }

    uint32_t slice = sl(key, lev);
    dprintf("-> %d: %02x\n", lev, slice);
    if (!lev)
    {
        if (n->nodes[slice])
        {
            *value = n->nodes[slice];
            n->nodes[slice] = 0;
            return !--n->nchildren && !n->only_key;
        }
        else
            return 0;
    }

    struct tcrnode *m = n->nodes[slice];
    if (m)
    {
        if (!nremove(c, m, lev-1, key, value))
            return 0;
        dprintf("freed @%d [%u] for %016lx\n", lev, slice, key);
        n->nodes[slice] = 0;
        m->only_val = 0; /* clear it for the next user */
        m->nodes[0] = c->deleted_node;
        c->deleted_node = m;
        #ifdef TRACEMEM
        memusage--;
        #endif
        n->nchildren--;
    }

    return !n->nchildren && !n->only_key;
}

void *FUNC(remove)(struct tcrhead *restrict n, uint64_t key)
{
    dprintf("remove(%016lx)\n", key);
    void* value = 0;
    pthread_mutex_lock(&n->mutex);
    write_poke(n);
    nremove(n, &n->root, LEVELS-1, key, &value);
    write_poke(n);
    pthread_mutex_unlock(&n->mutex);
    //display(&n->root, LEVELS-1);
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
        uint64_t nk;			\
        if ((l) && (nk = n->only_key))	\
        {				\
            if (nk == key)		\
                return n->only_val;	\
            else			\
                return NULL;		\
        }				\
        n = n->nodes[sl(key, (l))];	\
        if (!n)				\
            return NULL;		\
    }

/*
 * Yes, this loop is 100% identical as the function below.  Somehow, gcc at
 * -O1 and higher misoptimizes it much _slower_ than -Og/-O0, unless we copy
 * it to a separate function for the 2nd and further iterations.
 */
static void* get_slow(struct tcrhead *restrict h, uint64_t key)
{
    struct tcrnode *restrict n;
    uint64_t wrs1, wrs2;
retry:
    util_atomic_load_explicit64(&h->write_status, &wrs1, memory_order_acquire);
    if (wrs1 & 1)
    {
        sched_yield();
        goto retry;
    }

    n = (struct tcrnode*)h;
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
    util_atomic_load_explicit64(&h->write_status, &wrs2, memory_order_acquire);
    if (wrs1 != wrs2)
        goto retry;
    return n;
}

void* FUNC(get)(struct tcrhead *restrict h, uint64_t key)
{
#ifdef TRACEMEM
    util_fetch_and_add64(&gets, 1);
#endif
    dprintf("get(%016lx)\n", key);
    uint64_t wrs1, wrs2;
    util_atomic_load_explicit64(&h->write_status, &wrs1, memory_order_acquire);
    if (wrs1 & 1)
        return get_slow(h, key);
    struct tcrnode *restrict n = (struct tcrnode*)h;
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
    util_atomic_load_explicit64(&h->write_status, &wrs2, memory_order_acquire);
    return (wrs1 != wrs2) ? get_slow(h, key) : n;
}

void* FUNC(find_le)(struct tcrhead *restrict h, uint64_t key)
{
    fprintf(stderr, "Not implemented.\n");
    abort();
}

size_t FUNC(get_size)(struct tcrhead *restrict n)
{
#ifdef TRACEMEM
    return memusage*sizeof(struct tcrnode);
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

uint64_t FUNC(debug)(struct tcrhead *restrict n, uint64_t arg)
{
    return 0;
}
