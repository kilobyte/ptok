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
#define FUNC(x) tcradix_mutexSLICE_##x

#define printf(...)

//#define TRACEMEM

#ifdef TRACEMEM
static int64_t memusage=0;
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
    uint64_t pad[5]; // TODO: is avoiding cacheline dirtying worth it?
    pthread_mutex_t mutex;
};

#define TOP_EMPTY 0xffffffffffffffff

struct tcrhead *FUNC(new)(void)
{
#ifdef TRACEMEM
    memusage=1;
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

#ifndef printf
static void display(struct tcrnode *restrict n, int lev)
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
    memusage--;
#endif
}

void FUNC(delete)(struct tcrhead *restrict n)
{
    pthread_mutex_destroy(&n->mutex);
    teardown(&n->root, LEVELS-1);
#ifdef TRACEMEM
    if (memusage)
        fprintf(stderr, "==== memory leak: %ld left ====\n", memusage), abort();
#endif
}

static int insert(struct tcrnode *restrict n, int lev, uint64_t key, void *value);

static inline int insert_child(struct tcrnode *restrict n, int lev,
                               uint64_t key, void *value)
{
    uint32_t slice = sl(key, lev);
    struct tcrnode *restrict m;
    if ((m = n->nodes[slice]))
        return insert(m, lev-1, key, value);

    printf("new alloc\n");
    m = Zalloc(sizeof(struct tcrnode));
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
        printf("inserting 0 @%d\n", lev);
        int ret = insert(m, lev-1, key, value);
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

static int insert(struct tcrnode *restrict n, int lev, uint64_t key, void *value)
{
    printf("-> %d: %02x\n", lev, sl(key, lev));

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
            int ret = insert_child(n, lev, n->only_key, n->only_val);
            if (ret)
                return ret;
            n->only_key = 0;
        }
    }

    return insert_child(n, lev, key, value);
}

int FUNC(insert)(struct tcrhead *restrict n, uint64_t key, void *value)
{
    printf("insert(%016lx)\n", key);

    /* value of 0 is indistinguishable from "not existent" */
    if (!value)
        return 0;

    pthread_mutex_lock(&n->mutex);
    if (n->root.only_key == TOP_EMPTY && !n->root.nchildren)
    {
        n->root.only_val = value;
        n->root.only_key = key;
    }

    int ret = insert(&n->root, LEVELS-1, key, value);
    pthread_mutex_unlock(&n->mutex);
    if (ret)
        return ret;

#ifndef printf
    display(&n->root, LEVELS-1);
#endif
    return 0;
}

/* return 1 if we removed last subtree, making n empty */
static int nremove(struct tcrnode *restrict n, int lev, uint64_t key, void**restrict value)
{
    if (n->only_key == key && key)
    {
        n->only_key = 0;
        *value = n->only_val;
    }

    uint32_t slice = sl(key, lev);
    printf("-> %d: %02x\n", lev, slice);
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
    if (!m || !nremove(m, lev-1, key, value))
        return 0;

    n->nodes[slice] = 0;
    int was_only = !--n->nchildren && !n->only_key;
    printf("freed @%d [%u] for %016lx\n", lev, slice, key);
#ifdef TRACEMEM
    memusage--;
#endif
    Free(m);
    return was_only;
}

void *FUNC(remove)(struct tcrhead *restrict n, uint64_t key)
{
    printf("remove(%016lx)\n", key);
    void* value = 0;
    pthread_mutex_lock(&n->mutex);
    nremove(&n->root, LEVELS-1, key, &value);
    pthread_mutex_unlock(&n->mutex);
#ifndef printf
    display(&n->root, LEVELS-1);
#endif
    return value;
}

#define GETL(l) \
    if ((l)*SLICE < 64)			\
    {					\
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
    printf("get(%016lx)\n", key);
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

size_t FUNC(get_size)(struct tcrhead *restrict n)
{
#ifdef TRACEMEM
    return memusage*sizeof(struct tcrnode);
#else
    return 0;
#endif
}

uint64_t FUNC(debug)(struct tcrhead *restrict n, uint64_t arg)
{
    return 0;
}
