#include <pthread.h>
#include <errno.h>
#include "util.h"

/*
 * Are all possible values pointers?  If so, we could tag leaf nodes with a
 * bit (either LSB because they're pretty certainly aligned, or MSB because
 * 2⁶³ memory should be enough for anyone), and save half the memory and a
 * cacheline access.
 */

#define printf(...)

struct critbit
{
    struct critbit_node *root;
    pthread_mutex_t mutex;
};

struct critbit_node
{
    struct critbit_node *child[2];
    uint64_t path, bit;
};

struct critbit *critbit_new(void)
{
    struct critbit *c = Zalloc(sizeof(struct critbit));
    if (!c)
        return 0;
    pthread_mutex_init(&c->mutex, 0);
    return c;
}

static void delete_node(struct critbit_node *n)
{
    if (!n->bit)
        return;
    if (n->child[0])
        delete_node(n->child[0]);
    if (n->child[1])
        delete_node(n->child[1]);
}

void critbit_delete(struct critbit *c)
{
    if (c->root)
        delete_node(c->root);
    pthread_mutex_destroy(&c->mutex);
    Free(c);
}

#define UNLOCK pthread_mutex_unlock(&c->mutex)

int critbit_insert(struct critbit *c, uint64_t key, void *value)
{
    struct critbit_node *k = Zalloc(sizeof(struct critbit_node));
    if (!k)
        return ENOMEM;
    k->path = key;
    k->child[0] = value;

    pthread_mutex_lock(&c->mutex);
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

    struct critbit_node *m = Zalloc(sizeof(struct critbit_node));
    if (!m)
        return UNLOCK, Free(k), ENOMEM;

    uint64_t dir = !!(key&at);
    m->child[dir] = k;
    m->child[!dir] = n;
    m->bit = at;
    m->path = key & (at-1);
    *parent = m;

    return UNLOCK, 0;
}

void *critbit_remove(struct critbit *c, uint64_t key)
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
            // LEAKED!!!
            return UNLOCK, n->child[0];
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
    // n and k LEAKED!!!
    return UNLOCK, k->child[0];
}

void* critbit_get(struct critbit *c, uint64_t key)
{
    struct critbit_node *n = c->root;
    if (!n)
        return 0;
    while (n->bit)
        n = n->child[!!(n->bit & key)];
    return (n->path == key) ? n->child[0] : 0;
}

size_t critbit_get_size(struct critbit *c)
{
    return 0;
}

uint64_t critbit_debug(struct critbit *c, uint64_t arg)
{
    return 0;
}
