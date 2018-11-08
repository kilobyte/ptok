#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include "util.h"

//#define DEBUG_SPAM
#ifndef DEBUG_SPAM
#define printf(...) (void)0
#endif
#define FUNC(x) critnib_##x

//#define TRACEMEM

#ifdef TRACEMEM
static int64_t memusage=0;
static int64_t depths=0;
static int64_t gets=0;
#endif

struct critnib
{
    struct critnib_node *root;
    pthread_mutex_t mutex;
    struct critnib_node *deleted_node;
};

struct critnib_node
{
    struct critnib_node *child[16];
    uint64_t path;
    uint32_t shift;
};

#define ENDBIT 0xffffffff

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
#ifdef TRACEMEM
        memusage--;
#endif
        m=mm;
    }
    Free(c);
#ifdef TRACEMEM
    if (memusage)
        fprintf(stderr, "==== memory leak: %ld left ====\n", memusage), abort();
#endif
}

static void free_node(struct critnib *c, struct critnib_node *n)
{
    //n->child[0] = c->deleted_node;
    //c->deleted_node = n;
#ifdef TRACEMEM
    memusage--;
#endif
}

#define UNLOCK pthread_mutex_unlock(&c->mutex)

#ifndef DEBUG_SPAM
# define print_nib(...) (void)0
# define display(...) (void)0
#else
static void print_nib(uint64_t key, uint32_t sh)
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

    for (int s=0; s<n->shift; s+=4)
        printf(" ");
    if (n->path & (~0xfL << n->shift))
        printf("\e[1m[path not masked by shift]\e[0m ");
    print_nib(n->path, n->shift),printf("\n");
    for (int i=0; i<16; i++)
    {
        for (int s=0; s<n->shift; s+=4)
            printf(" ");
        printf("%x:", i);
        if (n->child[i]->shift <= n->shift)
            printf("\e[31m[non-monotonic shift!]");
        display(n->child[i]);
    }
    for (int s=0; s<n->shift; s+=4)
        printf(" ");
    printf("──────────────────────────────\n");
}
#endif

int FUNC(insert)(struct critnib *c, uint64_t key, void *value)
{
    /* We always need two nodes, so alloc them together to reduce malloc's
     * metadata.  Avoiding malloc inside the mutex is another bonus.
     */
    struct critnib_node *k = Zalloc(sizeof(struct critnib_node));
    if (!k)
        return ENOMEM;
    k->path = key;
    k->shift = ENDBIT;
    k->child[0] = value;

    pthread_mutex_lock(&c->mutex);
#ifdef TRACEMEM
    memusage++;
#endif
    printf("\e[33minsert %016lx\e[0m\n", key);
    struct critnib_node *n = c->root;
    if (n == &nullnode)
    {
        printf("- is new root\n");
        c->root = k;
        return UNLOCK, 0;
    }

    struct critnib_node **parent = &c->root, *prev = c->root;

    printf("Ω ");print_nib(n->path, n->shift);printf("\n");
    while (n->shift != ENDBIT && (key &~ (~0L << n->shift)) == n->path)
    {
        prev = n;
        parent = &n->child[(key >> n->shift) & 0xf];
        n = *parent;
        printf("• ");print_nib(n->path, n->shift);printf("\n");
    }

    if (n == &nullnode)
    {
        n = prev;
        printf("in-place update of ");print_nib(key, n->shift);printf("\n");
        n->child[(key >> n->shift) & 0xf] = k;
        display(c->root);
        return UNLOCK, 0;
    }

    uint64_t at = (n->path^key) & -(n->path^key);
    uint32_t sh = __builtin_ctzl(at) & ~3;

    printf(">> %u masked key=%016lx path=%016lx\n", n->shift, key &~ (~0xfL << n->shift), n->path);
    printf("diff of %016lx at %016lx >> %u\n", n->path^key, at, sh);

    printf("our side: "); print_nib(key&at, sh);
    printf(" nib: %lx, at: ", (key >> sh) & 0xf);
    print_nib(0xfL<<sh, sh);
    printf("\n");

    struct critnib_node *m = Zalloc(sizeof(struct critnib_node));
    if (!m)
    {
        free_node(c, k);
        return UNLOCK, ENOMEM;
    }
#ifdef TRACEMEM
    memusage++;
#endif

    for (int i=0; i<16; i++)
        m->child[i] = &nullnode;
    uint64_t dir = (key >> sh) & 0xf;
    m->child[dir] = k;
    m->child[(n->path >> sh) & 0xf] = n;
    m->shift = sh;
    m->path = key & ((1L<<sh)-1);
    *parent = m;

    display(c->root);
    return UNLOCK, 0;
}

void *FUNC(remove)(struct critnib *c, uint64_t key)
{
    printf("\e[33mremove %016lx\e[0m\n", key);
    pthread_mutex_lock(&c->mutex);

    struct critnib_node *n = c->root;
    if (!n)
        return UNLOCK, NULL;
    if (n->shift == ENDBIT)
    {
        if (n->path == key)
        {
            c->root = &nullnode;
            void* value = n->child[0];
            free_node(c, n);
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

    printf("R ");print_nib(n->path, n->shift);printf(" key ");print_nib(key, n->shift);printf("\n");
    n->child[(key >> n->shift) & 0xf] = &nullnode;

    int ochild = -1;
    for (int i=0; i<16; i++)
        if (n->child[i] != &nullnode)
        {
            if (ochild != -1)
            {
                void* value = k->child[0];
                free_node(c, k);
                return UNLOCK, value;
            }
            else
                ochild = i;
        }
    if (ochild == -1)
        ochild = 0;

    *n_parent = n->child[ochild];
    void* value = k->child[0];
    free_node(c, n);
    free_node(c, k);
    display(c->root);
    return UNLOCK, value;
}

void* FUNC(get)(struct critnib *c, uint64_t key)
{
    printf("\e[33mget %016lx\e[0m\n", key);
#ifdef TRACEMEM
    util_fetch_and_add64(&gets, 1);
    util_fetch_and_add64(&depths, 1);
#endif
    struct critnib_node *n = c->root;
    if (!n)
        return 0;
    while (n->shift != ENDBIT)
#ifdef TRACEMEM
        util_fetch_and_add64(&depths, 1),
#endif
        n = n->child[(key >> n->shift) & 0xf];
    return (n->path == key) ? n->child[0] : 0;
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
