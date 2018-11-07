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
//#define FUNC(x) tcradix_validSLICE_##x
#define FUNC(x) tcradix_valid_##x

//#define DEBUG_SPAM
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

struct tcrleaf
{
    struct { uint64_t key; void* val; } leaves[SLNODES];
};

struct tcrhead
{
    struct tcrnode root;

    /*
     * Probably doesn't matter, but let's not keep the write lock in the
     * hottest cacheline of reads.
     */
    uint64_t pad[5];
    pthread_mutex_t mutex;
    struct tcrleaf *deleted_leaf;
    struct tcrnode *deleted_node;
};

#define TOP_EMPTY 0xffffffffffffffff

struct tcrhead *FUNC(new)(void)
{
#ifdef TRACEMEM
    memusage=sizeof(struct tcrhead);
    depths=gets=0;
#endif
    struct tcrhead *n = Zalloc(sizeof(struct tcrhead));
    if (!n)
        return 0;
    n->root.only_key = TOP_EMPTY;
    pthread_mutex_init(&n->mutex, 0);
    return n;
}

static inline uint32_t sl(uint64_t key, int lev)
{
    return key>>(lev*SLICE) & (SLNODES-1);
}

#ifdef DEBUG_SPAM
static void display(struct tcrnode *restrict n, int lev)
{
    for (int k=lev; k<LEVELS; k++)
        printf(" ");

    if (lev)
    {
        printf("@%d only_key=%016lx, only_val=%016lx, nchildren=%lu\n", lev,
            n->only_key, (uint64_t)n->only_val, n->nchildren);
        for (uint32_t i=0; i<SLNODES; i++)
            if (n->nodes[i])
            {
                for (int k=lev; k<LEVELS; k++)
                    printf(" ");
                printf("• %02x:\n", i);
                display(n->nodes[i], lev-1);
            }
    }
    else
    {
        struct tcrleaf *l = (void*)n;
        printf("LEAF\n");
        for (uint32_t i=0; i<SLNODES; i++)
            if (l->leaves[i].val)
            {
                for (int k=lev; k<LEVELS; k++)
                    printf(" ");
                printf("❧ %02x: [%016lx]:{%s}\n", i, l->leaves[i].key, (const char*)l->leaves[i].val);
            }
    }
}
#else
#define display(...)
#endif

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
    memusage-= lev? sizeof(struct tcrnode) : sizeof(struct tcrleaf);
#endif
}

void FUNC(delete)(struct tcrhead *restrict n)
{
    pthread_mutex_destroy(&n->mutex);
    for (struct tcrleaf *l = n->deleted_leaf; l; )
    {
        struct tcrleaf *ll=(void*)l->leaves[0].key;
        Free(l);
#ifdef TRACEMEM
        memusage -= sizeof(struct tcrleaf);
#endif
        l=ll;
    }
    for (struct tcrnode *m = n->deleted_node; m; )
    {
        struct tcrnode *mm=m->nodes[0];
        Free(m);
#ifdef TRACEMEM
        memusage -= sizeof(struct tcrnode);
#endif
        m=mm;
    }
    teardown(&n->root, LEVELS-1);
#ifdef TRACEMEM
    memusage -= sizeof(struct tcrhead)-sizeof(struct tcrnode);
    if (memusage)
        fprintf(stderr, "==== memory leak: %ld left ====\n", memusage), abort();
#endif
}

static struct tcrleaf *alloc_leaf(struct tcrhead *c)
{
    if (!c->deleted_leaf)
        return Zalloc(sizeof(struct tcrleaf));
    struct tcrleaf *n = c->deleted_leaf;
    c->deleted_leaf = (struct tcrleaf *)n->leaves[0].key;
    n->leaves[0].key = 0;

#ifdef DEBUG_SPAM
    for (size_t i=0; i<sizeof(*n); i++)
        if (((const char*)(n))[i])
            fprintf(stderr, "reclaimed leaf not clean at byte %zx\n", i);
#endif

    return n;
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

static int insert(struct tcrhead *restrict c, struct tcrnode *restrict n, int lev, uint64_t key, void *value);

static inline int insert_child(struct tcrhead *restrict c, struct tcrnode *restrict n, int lev,
                               uint64_t key, void *value)
{
    uint32_t slice = sl(key, lev);
    struct tcrnode *restrict m;

    if ((m = n->nodes[slice]))
        return insert(c, m, lev-1, key, value);

    dprintf("new alloc @%d %02x key=%016lx\n", lev, slice, key);
    if (lev == 1)
    {
        dprintf("-- for lev=0, key=%016lx\n", key);
        struct tcrleaf *restrict l = alloc_leaf(c);
        if (!l)
            return ENOMEM;
#ifdef TRACEMEM
        memusage+=sizeof(struct tcrleaf);
#endif
        uint32_t lslice = sl(key, 0);
        l->leaves[lslice].val = value;
        l->leaves[lslice].key = key;
        n->nodes[slice] = (void*)l;
        n->nchildren++;
        return 0;
    }

    m = alloc_node(c);
#ifdef TRACEMEM
    memusage+=sizeof(struct tcrnode);
#endif
    if (!m)
        return ENOMEM;

    m->only_key = key;
    m->only_val = value;
    m->nchildren = 0;
    if (!key) // nasty special case: key of 0
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

static int insert(struct tcrhead *restrict c, struct tcrnode *restrict n, int lev, uint64_t key, void *value)
{
    dprintf("@%d: %02x\n", lev, sl(key, lev));

    if (!lev)
    {
        struct tcrleaf *l = (void*)n;
        uint32_t slice = sl(key, lev);
        l->leaves[slice].val = value;
        l->leaves[slice].key = key;
        return 0;
    }

    if (n->only_key)
    {
        if (n->only_key == key)
        {
            n->only_val = value;
            return 0;
        }
        else
        {
            // need to materialize the has-been-only node
            dprintf("◘ ");
            int ret = insert_child(c, n, lev, n->only_key, n->only_val);
            if (ret)
                return ret;
            n->only_key = 0;
        }
    }

    dprintf("⦿ ");
    return insert_child(c, n, lev, key, value);
}

int FUNC(insert)(struct tcrhead *restrict n, uint64_t key, void *value)
{
    dprintf("\e[33minsert(%016lx)\e[0m\n", key);

    /* value of 0 is indistinguishable from "not existent" */
    if (!value)
        return 0;

    pthread_mutex_lock(&n->mutex);
    if (n->root.only_key == TOP_EMPTY && !n->root.nchildren)
    {
        n->root.only_val = value;
        n->root.only_key = key;
    }

    int ret = insert(n, &n->root, LEVELS-1, key, value);
    pthread_mutex_unlock(&n->mutex);
    if (ret)
        return ret;

    display(&n->root, LEVELS-1);
    return 0;
}

/* return 1 if we removed last subtree, making n empty */
static int nremove(struct tcrhead *c, struct tcrnode *restrict n, int lev, uint64_t key, void**restrict value)
{
    uint32_t slice = sl(key, lev);
    dprintf("-> %d: %02x\n", lev, slice);
    if (!lev)
    {
        struct tcrleaf *l = (void*)n;
        *value = l->leaves[slice].val;
        l->leaves[slice].val = 0;
        l->leaves[slice].key = 0;
        for (uint32_t i = 0; i<SLNODES; i++)
            if (l->leaves[i].val)
                return 0;
        return 1;
    }

    if (n->only_key == key && key)
    {
        n->only_key = 0;
        *value = n->only_val;
    }

    struct tcrnode *m = n->nodes[slice];
    if (m)
    {
        if (!nremove(c, m, lev-1, key, value))
            return 0;
        dprintf("freed @%d [%u] for %016lx\n", lev, slice, key);
        n->nodes[slice] = 0;
        if (lev==1)
        {
            struct tcrleaf *l = (void*)m;
            l->leaves[0].key = (uint64_t)c->deleted_leaf;
            c->deleted_leaf = l;
#ifdef TRACEMEM
            memusage-=sizeof(struct tcrleaf);
#endif
        }
        else
        {
            m->only_val = 0; /* clear it for the next user */
            m->nodes[0] = c->deleted_node;
            c->deleted_node = m;
#ifdef TRACEMEM
            memusage-=sizeof(struct tcrnode);
#endif
        }
        n->nchildren--;
    }

    return !n->nchildren && !n->only_key;
}

void *FUNC(remove)(struct tcrhead *restrict n, uint64_t key)
{
    dprintf("\e[33mremove(%016lx)\e[0m\n", key);
    void* value = 0;
    pthread_mutex_lock(&n->mutex);
    nremove(n, &n->root, LEVELS-1, key, &value);
    pthread_mutex_unlock(&n->mutex);
    display(&n->root, LEVELS-1);
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
                return 0;		\
        }				\
        n = n->nodes[sl(key, (l))];	\
        if (!n)				\
            return 0;			\
    }

void* FUNC(get)(struct tcrnode *restrict n, uint64_t key)
{
#ifdef TRACEMEM
    util_fetch_and_add64(&gets, 1);
#endif
    dprintf("\e[33mget(%016lx)\e[0m\n", key);
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
    INCDEPTHS;
    struct tcrleaf *l = (void*)n;
    int slice = sl(key, 0);
    void *val = l->leaves[slice].val;
    return (l->leaves[slice].key==key) ? val : 0;
}

size_t FUNC(get_size)(struct tcrhead *restrict n)
{
#ifdef TRACEMEM
    return memusage;
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
