#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"

//#define SLICE 8
#define SLNODES (1<<(SLICE))
#define LEVELS ((63+SLICE)/SLICE)
#define FUNC(x) tcradixSLICE_##x

#define printf(...)

struct tcrnode
{
    struct tcrnode* nodes[SLNODES];
    uint64_t only_key;
    void*    only_val;
};

#define TOP_EMPTY 0xffffffffffffffff

struct tcrnode *FUNC(new)(void)
{
    struct tcrnode *n = Zalloc(sizeof(struct tcrnode));
    if (n)
        n->only_key = TOP_EMPTY;
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
    printf("only_key=%016lx, only_val=%016lx\n", n->only_key, (uint64_t)n->only_val);
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
        for (uint32_t i=0; i<SLNODES; i++)
            if (n->nodes[i])
                teardown(n->nodes[i], lev-1);
    }
    Free(n);
}

void FUNC(delete)(struct tcrnode *restrict n)
{
    teardown(n, LEVELS-1);
}

static int insert(struct tcrnode *restrict n, int lev, uint64_t key, void *value)
{
tail_recurse:;
    uint32_t slice = sl(key, lev);
    printf("-> %d: %02x\n", lev, slice);

    if (!lev)
    {
        n->nodes[slice] = value;
        return 0;
    }

retry:
    if (n->only_key)
    {
        if (n->only_key == key)
            n->only_val = value;
        else
            n->only_key = 0;
    }

    struct tcrnode *restrict m;
    if ((m = n->nodes[slice]))
    {
        n = m;
        lev--;
        goto tail_recurse;
    }

    printf("new alloc\n");
    m = Zalloc(sizeof(struct tcrnode));
    if (!m)
        return ENOMEM;

    int ret = insert(m, lev-1, key, value);
    if (ret)
    {
        Free(m);
        return ret;
    }

    m->only_key = key;
    m->only_val = value;
    if (util_bool_compare_and_swap64(&n->nodes[slice], 0, m))
        return 0;
    /* Someone else just created this subtree for us. */
    teardown(m, lev-1);
    goto retry;
}

int FUNC(insert)(struct tcrnode *restrict n, uint64_t key, void *value)
{
    printf("insert(%016lx)\n", key);

    if (n->only_key == TOP_EMPTY)
    {
        /* A slight race in a pathological case:
           * thread 1 creates then deletes an entry with oid=0xffffffffffffffff
           * thread 2 creates something
           and both race in this tiny section.
         */
        if (util_bool_compare_and_swap64(&n->only_val, 0, value))
            if (!util_bool_compare_and_swap64(&n->only_key, TOP_EMPTY, key))
                n->only_key = 0;
    }

    int ret = insert(n, LEVELS-1, key, value);
    if (ret)
        return ret;

#ifndef printf
    display(n, LEVELS-1);
#endif
    return 0;
}

void *FUNC(remove)(struct tcrnode *restrict n, uint64_t key)
{
    printf("remove(%016lx)\n", key);
    for (int lev = LEVELS-1; ; lev--)
    {
        if (n->only_key == key)
            n->only_key = 0;
        uint32_t slice = sl(key, lev);
        printf("-> %d: %02x\n", lev, slice);
        if (lev)
            n = n->nodes[slice];
        else
        {
            void* was = n->nodes[slice];
            n->nodes[slice] = 0;
            return was;
        }
    }
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

size_t FUNC(get_size)(struct tcrnode *restrict n)
{
    return 0;
}