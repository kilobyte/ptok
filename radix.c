#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"

//#define SLICE 8
#define SLNODES (1<<(SLICE))
#define LEVELS ((63+SLICE)/SLICE)
#define FUNC(x) radixSLICE_##x

#define printf(...)

struct rnode
{
    struct rnode* nodes[SLNODES];
};

#define TOP_EMPTY 0xffffffffffffffff

struct rnode *FUNC(new)(void)
{
    return Zalloc(sizeof(struct rnode));
}

static inline uint32_t sl(uint64_t key, int lev)
{
    return key>>(lev*SLICE) & (SLNODES-1);
}

#ifndef printf
static void display(struct rnode *restrict n, int lev)
{
    for (int k=lev; k<LEVELS; k++)
        printf(" ");
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

static void teardown(struct rnode *restrict n, int lev)
{
    if (lev)
    {
        for (uint32_t i=0; i<SLNODES; i++)
            if (n->nodes[i])
                teardown(n->nodes[i], lev-1);
    }
    Free(n);
}

void FUNC(delete)(struct rnode *restrict n)
{
    teardown(n, LEVELS-1);
}

static int insert(struct rnode *restrict n, int lev, uint64_t key, void *value)
{
tail_recurse:;
    uint32_t slice = sl(key, lev);
    printf("-> %d: %02x\n", lev, slice);

    if (!lev)
    {
        n->nodes[slice] = value;
        return 0;
    }

retry:;
    struct rnode *restrict m;
    if ((m = n->nodes[slice]))
    {
        n = m;
        lev--;
        goto tail_recurse;
    }

    printf("new alloc\n");
    m = Zalloc(sizeof(struct rnode));
    if (!m)
        return ENOMEM;

    int ret = insert(m, lev-1, key, value);
    if (ret)
    {
        Free(m);
        return ret;
    }

    if (util_bool_compare_and_swap64(&n->nodes[slice], 0, m))
        return 0;
    /* Someone else just created this subtree for us. */
    teardown(m, lev-1);
    goto retry;
}

int FUNC(insert)(struct rnode *restrict n, uint64_t key, void *value)
{
    printf("insert(%016lx)\n", key);
    int ret = insert(n, LEVELS-1, key, value);
    if (ret)
        return ret;

#ifndef printf
    display(n, LEVELS-1);
#endif
    return 0;
}

void *FUNC(remove)(struct rnode *restrict n, uint64_t key)
{
    printf("remove(%016lx)\n", key);
    for (int lev = LEVELS-1; ; lev--)
    {
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

void* FUNC(get)(struct rnode *restrict n, uint64_t key)
{
    // TODO: unroll the loop
    printf("get(%016lx)\n", key);
    for (int lev = LEVELS-1; lev>=0; lev--)
    {
        uint32_t slice = sl(key, lev);
        printf("-> %d: %02x\n", lev, slice);
        n = n->nodes[slice];
        if (!n)
        {
            printf("---> missing\n");
            return 0;
        }
    }
    return n;
}

size_t FUNC(get_size)(struct rnode *restrict n)
{
    return 0;
}
