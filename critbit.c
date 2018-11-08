#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include "util.h"

/*
 * Are all possible values pointers?  If so, we could tag leaf nodes with a
 * bit (either LSB because they're pretty certainly aligned, or MSB because
 * 2⁶³ memory should be enough for anyone), and save half the memory and a
 * cacheline access.
 */

#define printf(...)
#define FUNC(x) critbit_##x

//#define TRACEMEM

#ifdef TRACEMEM
static int64_t memusage=0;
static int64_t depths=0;
static int64_t gets=0;
#endif

struct critbit
{
    struct critbit_node *root;
    pthread_mutex_t mutex;
    struct critbit_node *deleted_node;
};

struct critbit_node
{
    struct critbit_node *child[2];
    uint64_t path, bit;
};

struct critbit *FUNC(new)(void)
{
    struct critbit *c = Zalloc(sizeof(struct critbit));
    if (!c)
        return 0;
#ifdef TRACEMEM
    memusage=1;
    depths=gets=0;
#endif
    pthread_mutex_init(&c->mutex, 0);
    return c;
}

static void free_node(struct critbit *c, struct critbit_node *n)
{
    n->child[0] = c->deleted_node;
    c->deleted_node = n;
}

static void delete_node(struct critbit *c, struct critbit_node *n)
{
    if (!n->bit)
        return free_node(c, n); /* can't free it immediately */
    if (n->child[0])
        delete_node(c, n->child[0]);
    if (n->child[1])
        delete_node(c, n->child[1]);
}

void FUNC(delete)(struct critbit *c)
{
    if (c->root)
        delete_node(c, c->root);
    pthread_mutex_destroy(&c->mutex);
    for (struct critbit_node *m = c->deleted_node; m; )
    {
        struct critbit_node *mm=m->child[0];
        Free(m);
#ifdef TRACEMEM
        memusage -= 2;
#endif
        m=mm;
    }
    Free(c);
}

static struct critbit_node *alloc_node(struct critbit *c)
{
    if (1 || !c->deleted_node)
        return Zalloc(sizeof(struct critbit_node)*2);
    struct critbit_node *n = c->deleted_node;
    c->deleted_node = n->child[0];
    n->child[0] = 0;
    n->child[1] = 0;
    n->path = 0;
    n->bit = 0;

#ifdef DEBUG_SPAM
    for (size_t i=0; i<sizeof(*n); i++)
        if (((const char*)(n))[i])
            fprintf(stderr, "reclaimed node not clean at byte %zx\n", i);
#endif

    return n;
}

#define UNLOCK pthread_mutex_unlock(&c->mutex)

int FUNC(insert)(struct critbit *c, uint64_t key, void *value)
{
    /* We always need two nodes, so alloc them together to reduce malloc's
     * metadata.  Avoiding malloc inside the mutex is another bonus.
     */
    struct critbit_node *k = alloc_node(c);
    if (!k)
        return ENOMEM;
    k->path = key;
    k->child[0] = value;

    pthread_mutex_lock(&c->mutex);
#ifdef TRACEMEM
    memusage+=2;
#endif
    printf("insert %016lx\n", key);
    struct critbit_node *n = c->root;
    if (!n)
    {
        printf("- is new root\n");
        c->root = k;
        return UNLOCK, 0;
    }

    struct critbit_node **parent = &c->root;
    printf("Ω %016lx %016lx\n", n->path, n->bit);
    while (n->bit && ((key & (n->bit-1)) == n->path))
    {
        parent = &n->child[!!(n->bit & key)];
        n = *parent;
        printf("• %016lx %016lx\n", n->path, n->bit);
    }

    uint64_t at = (n->path^key) & -(n->path^key);
    printf("diff of %016lx at %016lx\n", n->path^key, at);
    printf("our side: %016lx\n", key&at);

    struct critbit_node *m = k+1;

    uint64_t dir = !!(key&at);
    m->child[dir] = k;
    m->child[!dir] = n;
    m->bit = at;
    m->path = key & (at-1);
    *parent = m;

    return UNLOCK, 0;
}

void *FUNC(remove)(struct critbit *c, uint64_t key)
{
    pthread_mutex_lock(&c->mutex);

    struct critbit_node *n = c->root;
    if (!n)
        return UNLOCK, NULL;
    if (!n->bit)
    {
        if (n->path == key)
        {
            c->root = 0;
            void* value = n->child[0];
            free_node(c, n);
            return UNLOCK, value;
        }
        return UNLOCK, NULL;
    }
    struct critbit_node **k_parent = &c->root, **n_parent = &c->root, *k = n;

    while (k->bit)
    {
        n_parent = k_parent;
        n = k;
        k_parent = &k->child[!!(k->bit & key)];
        k = *k_parent;
    }
    if (k->path != key)
        return UNLOCK, NULL;

    printf("R %016lx %016lx\n", n->path, n->bit);
    if (k_parent == &n->child[0])
        *n_parent = n->child[1];
    else
        *n_parent = n->child[0];
    void* value = k->child[0];
    free_node(c, k);
    return UNLOCK, value;
}

void* FUNC(get)(struct critbit *c, uint64_t key)
{
#ifdef TRACEMEM
    util_fetch_and_add64(&gets, 1);
    util_fetch_and_add64(&depths, 1);
#endif
    struct critbit_node *n = c->root;
    if (!n)
        return 0;
    while (n->bit)
#ifdef TRACEMEM
        util_fetch_and_add64(&depths, 1),
#endif
        n = n->child[!!(n->bit & key)];
    return (n->path == key) ? n->child[0] : 0;
}

void* FUNC(find_le)(struct critbit *restrict c, uint64_t key)
{
    fprintf(stderr, "Not implemented.\n");
    abort();
}

size_t FUNC(get_size)(struct critbit *c)
{
#ifdef TRACEMEM
    return memusage*sizeof(struct critbit_node);
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

uint64_t FUNC(debug)(struct critbit *c, uint64_t arg)
{
    return 0;
}
