#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"

#define SLICE 8
#define SLNODES (1<<(SLICE))
#define LEVELS ((63+SLICE)/SLICE)

#define printf(...)

struct tcrnode
{
    struct tcrnode* nodes[SLNODES];
    uint64_t only_key;
    void*    only_val;
};

#define TOP_EMPTY 0xffffffffffffffff

struct tcrnode *tcradix_new(void)
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

void tcradix_delete(struct tcrnode *restrict n)
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

int tcradix_insert(struct tcrnode *restrict n, uint64_t key, void *value)
{
    printf("insert(%016lx)\n", key);
    int ret = insert(n, LEVELS-1, key, value);
    if (ret)
        return ret;

    if (n->only_key == TOP_EMPTY) //FIXME
    {
        /* might be slightly racey */
        if (util_bool_compare_and_swap64(&n->only_val, 0, value))
            if (!util_bool_compare_and_swap64(&n->only_key, TOP_EMPTY, key))
                n->only_key = 0;
    }
#ifndef printf
    display(n, LEVELS-1);
#endif
    return 0;
}

void *tcradix_remove(struct tcrnode *restrict n, uint64_t key)
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

void* tcradix_get(struct tcrnode *restrict n, uint64_t key)
{
    // TODO: unroll the loop
    printf("get(%016lx)\n", key);
    for (int lev = LEVELS-1; lev>=0; lev--)
    {
        uint64_t nk;
        if (lev && (nk = n->only_key))
        {
            if (nk == key)
            {
                printf("----> direct match\n");
                return n->only_val;
            }
            else
            {
                printf("-!--> direct mismatch\n");
                return 0;
            }
        }
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

size_t tcradix_get_size(struct tcrnode *restrict n)
{
    return 0;
}
