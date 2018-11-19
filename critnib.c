#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include "util.h"

//#define DEBUG_SPAM
#ifdef DEBUG_SPAM
#define dprintf(...) printf(__VA_ARGS__)
#else
#define dprintf(...) (void)0
#endif
#define FUNC(x) critnib_##x

//#define TRACEMEM

#ifdef TRACEMEM
static int64_t memusage=0;
static int64_t depths=0;
static int64_t gets=0;
#endif

/*
 * A node that has been deleted is left untouched for this many delete
 * cycles.  Reads have guaranteed correctness if they took no longer than
 * DELETED_LIFE concurrent deletes, otherwise they notice something is
 * wrong and restart.  The memory of deleted nodes is never freed to
 * malloc nor their pointers lead anywhere wrong, thus a stale read will
 * (temporarily) get a wrong answer but won't crash.
 *
 * There's no need to count writes as they never interfere with reads.
 *
 * Allowing stale reads (of arbitrarily old writes or of deletes less than
 * DELETED_LIFE old) might sound counterintuitive, but it doesn't affect
 * semantics in any way: the thread could have been stalled just after
 * returning from our code.  Thus, the guarantee is: the result of get() or
 * find_le() is a value that was current at any point between the call
 * started and ended.
 *
 */
#define DELETED_LIFE 16

struct critnib_node
{
    struct critnib_node *child[16];
    uint64_t path;
    int32_t shift;
};

struct critnib
{
    struct critnib_node *root;
    struct critnib_node *deleted_node;
    struct critnib_node *pending_dels[DELETED_LIFE][2];
    uint64_t volatile write_status;
    pthread_mutex_t mutex;
};

#define ENDBIT -1

static struct critnib_node nullnode =
{
    {0,},
    0, ENDBIT
};

struct critnib *FUNC(new)(void)
{
    struct critnib *c = Zalloc(sizeof(struct critnib));
    if (!c)
        return 0;
    c->root = &nullnode;
#ifdef TRACEMEM
    memusage=0;
    depths=gets=0;
#endif
    pthread_mutex_init(&c->mutex, 0);
    return c;
}

static void delete_node(struct critnib_node *n)
{
    if (n == &nullnode)
        return;

    if (n->shift != ENDBIT)
    {
        for (int i=0; i<16; i++)
            if (n->child[i])
                delete_node(n->child[i]);
    }
#ifdef TRACEMEM
    memusage--;
#endif
    Free(n);
}

void FUNC(delete)(struct critnib *c)
{
    if (c->root)
        delete_node(c->root);
    pthread_mutex_destroy(&c->mutex);
    for (struct critnib_node *m = c->deleted_node; m; )
    {
        struct critnib_node *mm=m->child[0];
        Free(m);
        m=mm;
    }
    for (int i=0; i<DELETED_LIFE; i++)
        for (int j=0; j<2; j++)
            if (c->pending_dels[i][j])
                Free(c->pending_dels[i][j]);
    Free(c);
#ifdef TRACEMEM
    if (memusage)
        fprintf(stderr, "==== memory leak: %ld left ====\n", memusage), abort();
#endif
}

static void free_node(struct critnib *c, struct critnib_node *n)
{
    if (!n)
        return;
    n->child[0] = c->deleted_node;
    c->deleted_node = n;
}

static struct critnib_node* alloc_node(struct critnib *c)
{
#ifdef TRACEMEM
    memusage++;
#endif
    if (!c->deleted_node)
        return Malloc(sizeof(struct critnib_node));
    struct critnib_node *n = c->deleted_node;
    c->deleted_node = n->child[0];
    return n;
}

#define UNLOCK pthread_mutex_unlock(&c->mutex)

#ifndef DEBUG_SPAM
# define print_nib(...) (void)0
# define display(...) (void)0
#else
static void print_nib(uint64_t key, int32_t sh)
{
    if (sh == ENDBIT)
        return (void)printf("\e[35m%016lx\e[0m >> ENDBIT", key);

    if (sh<=60)
        printf("\e[0;34;1m%0*lx", 15-sh/4, key>>(sh+4));
    printf("\e[0;37;1m%lx", (key>>sh)&0xf);
    if (sh)
        printf("\e[0;32m%0*lx", sh/4, key&~(~0L<<sh));
    printf("\e[0m[%u]", sh);
}

static void display(struct critnib_node *n)
{
    if (n->shift == ENDBIT)
    {
        if (n == &nullnode)
            printf("∅\n");
        else
            printf("»»»»»»»»»»»»»»»» %016lx -> %016lx\n", n->path, (uint64_t)n->child[0]);
        return;
    }

    for (int s=n->shift; s<64; s+=4)
        printf(" ");
    if (n->path &~ (~0L << n->shift))
        printf("\e[1m[path not masked by shift]\e[0m ");
    print_nib(n->path, n->shift),printf("\n");
    for (int i=0; i<16; i++)
    {
        for (int s=n->shift; s<64; s+=4)
            printf(" ");
        printf("%x:", i);
        if (n->child[i]->shift >= n->shift)
            printf("\e[31m[non-monotonic shift!]");
        display(n->child[i]);
    }
    for (int s=n->shift; s<64; s+=4)
        printf(" ");
    printf("──────────────────────────────\n");
}
#endif

int FUNC(insert)(struct critnib *c, uint64_t key, void *value)
{
    pthread_mutex_lock(&c->mutex);
    struct critnib_node *k = alloc_node(c);
    if (!k)
        return UNLOCK, ENOMEM;
    k->path = key;
    k->shift = ENDBIT;
    k->child[0] = value;

    dprintf("\e[33minsert %016lx\e[0m\n", key);
    struct critnib_node *n = c->root;
    if (n == &nullnode)
    {
        dprintf("- is new root\n");
        c->root = k;
        display(k);
        return UNLOCK, 0;
    }

    struct critnib_node **parent = &c->root, *prev = c->root;

    dprintf("Ω ");print_nib(n->path, n->shift);dprintf("\n");
    while (n->shift != ENDBIT && (key & (~0xfL << n->shift)) == n->path)
    {
        prev = n;
        parent = &n->child[(key >> n->shift) & 0xf];
        n = *parent;
        dprintf("• ");print_nib(n->path, n->shift);dprintf("\n");
    }

    if (n == &nullnode)
    {
        n = prev;
        dprintf("in-place update of ");print_nib(key, n->shift);dprintf("\n");
        util_atomic_store_explicit64(&n->child[(key >> n->shift) & 0xf], k, memory_order_release);
        display(c->root);
        return UNLOCK, 0;
    }

    uint64_t at = n->path^key;
    int32_t sh = 60 - (__builtin_clzl(at) & ~3);

    dprintf(">> %u masked key=%016lx path=%016lx\n", n->shift, key &~ (~0xfL >> n->shift), n->path);
    dprintf("diff of %016lx at %016lx >> %u\n", n->path^key, at, sh);

    dprintf("our side: "); print_nib(key&at, sh);
    dprintf(" nib: %lx, at: ", (key >> sh) & 0xf);
    print_nib(0xfL<<sh, sh);
    dprintf("\n");

    struct critnib_node *m = alloc_node(c);
    if (!m)
    {
        free_node(c, k);
        return UNLOCK, ENOMEM;
    }

    for (int i=0; i<16; i++)
        m->child[i] = &nullnode;
    uint64_t dir = (key >> sh) & 0xf;
    m->child[dir] = k;
    m->child[(n->path >> sh) & 0xf] = n;
    m->shift = sh;
    m->path = key & (~0xfL << sh);
    util_atomic_store_explicit64(parent, m, memory_order_release);

    display(c->root);
    return UNLOCK, 0;
}

void *FUNC(remove)(struct critnib *c, uint64_t key)
{
    dprintf("\e[33mremove %016lx\e[0m\n", key);
    pthread_mutex_lock(&c->mutex);

    struct critnib_node *n = c->root;
    if (!n)
        return UNLOCK, NULL;
    uint64_t del = util_fetch_and_add64(&c->write_status, 1) % DELETED_LIFE;
    free_node(c, c->pending_dels[del][0]);
    free_node(c, c->pending_dels[del][1]);
    c->pending_dels[del][1] = c->pending_dels[del][0] = 0;
    if (n->shift == ENDBIT)
    {
        if (n->path == key)
        {
            util_atomic_store_explicit64(&c->root, &nullnode, memory_order_release);
            void* value = n->child[0];
            c->pending_dels[del][0] = n;
#ifdef TRACEMEM
            memusage--;
#endif
            return UNLOCK, value;
        }
        return UNLOCK, NULL;
    }
    struct critnib_node **k_parent = &c->root, **n_parent = &c->root, *k = n;

    while (k->shift != ENDBIT)
    {
        n_parent = k_parent;
        n = k;
        k_parent = &k->child[(key >> k->shift) & 0xf];
        k = *k_parent;
    }
    if (k->path != key)
        return UNLOCK, NULL;

    dprintf("R ");print_nib(n->path, n->shift);dprintf(" key ");print_nib(key, n->shift);dprintf("\n");
    util_atomic_store_explicit64(&n->child[(key >> n->shift) & 0xf], &nullnode, memory_order_release);

    int ochild = -1;
    for (int i=0; i<16; i++)
        if (n->child[i] != &nullnode)
        {
            if (ochild != -1)
            {
                void* value = k->child[0];
                c->pending_dels[del][0] = k;
#ifdef TRACEMEM
                memusage--;
#endif
                return UNLOCK, value;
            }
            else
                ochild = i;
        }
    if (ochild == -1)
        ochild = 0;

    util_atomic_store_explicit64(n_parent, n->child[ochild], memory_order_release);
    void* value = k->child[0];
    c->pending_dels[del][0] = n;
    c->pending_dels[del][1] = k;
#ifdef TRACEMEM
    memusage-=2;
#endif
    display(c->root);
    return UNLOCK, value;
}

void* FUNC(get)(struct critnib *c, uint64_t key)
{
    dprintf("\e[33mget %016lx\e[0m\n", key);
#ifdef TRACEMEM
    util_fetch_and_add64(&gets, 1);
    util_fetch_and_add64(&depths, 1);
#endif
    uint64_t wrs1, wrs2;
retry:
    util_atomic_load_explicit64(&c->write_status, &wrs1, memory_order_acquire);
    struct critnib_node *n = c->root;
    if (!n)
        return 0;
    while (n->shift != ENDBIT)
#ifdef TRACEMEM
        util_fetch_and_add64(&depths, 1),
#endif
        n = n->child[(key >> n->shift) & 0xf];
    void* res = (n->path == key) ? n->child[0] : 0;
    util_atomic_load_explicit64(&c->write_status, &wrs2, memory_order_acquire);
    if (wrs1 + DELETED_LIFE <= wrs2)
        goto retry;
    return res;
}

static void* find_le(struct critnib_node *restrict n, uint64_t key)
{
    if (n->shift == ENDBIT)
        return (n->path <= key) ? n->child[0] : NULL;

    if ((key ^ n->path) >> (n->shift) & ~0xfL)
    {
        if (n->path < key)
            goto dive;
        return NULL;
    }

    int nib = (key >> n->shift) & 0xf;
    void* value = find_le(n->child[nib], key);
    if (value)
        return value;
    for (nib--; nib >= 0; nib--)
        if (n->child[nib] != &nullnode)
            goto deeper;
    return NULL;

deeper:
    n = n->child[nib];
    if (n->shift == ENDBIT)
        return n->child[0];
dive:
    for (nib=0xf; nib >= 0; nib--)
        if (n->child[nib] != &nullnode)
            goto deeper;
    return NULL;
}

void* FUNC(find_le)(struct critnib *restrict c, uint64_t key)
{
    uint64_t wrs1, wrs2;
retry:
    util_atomic_load_explicit64(&c->write_status, &wrs1, memory_order_acquire);
    void* res = find_le(c->root, key);
    util_atomic_load_explicit64(&c->write_status, &wrs2, memory_order_acquire);
    if (wrs1 + DELETED_LIFE <= wrs2)
        goto retry;
    return res;
}

size_t FUNC(get_size)(struct critnib *c)
{
#ifdef TRACEMEM
    return memusage*sizeof(struct critnib_node);
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

uint64_t FUNC(debug)(struct critnib *c, uint64_t arg)
{
    return 0;
}
