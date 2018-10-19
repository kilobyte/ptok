#include <pthread.h>
#include <stdlib.h>

#include "cuckoo.h"

struct cs
{
    struct cuckoo *c;
    pthread_mutex_t mutex;
};

struct cs *cuckoo_mutex_new(void)
{
    struct cs *c = malloc(sizeof(struct cs));
    c->c=cuckoo_new();
    pthread_mutex_init(&c->mutex, 0);
    return c;
}

void cuckoo_mutex_delete(struct cs *c)
{
    pthread_mutex_destroy(&c->mutex);
    cuckoo_delete(c->c);
}

int cuckoo_mutex_insert(struct cs *c, uint64_t key, void *value)
{
    pthread_mutex_lock(&c->mutex);
    int ret=cuckoo_insert(c->c, key, value);
    pthread_mutex_unlock(&c->mutex);
    return ret;
}

void *cuckoo_mutex_remove(struct cs *c, uint64_t key)
{
    pthread_mutex_lock(&c->mutex);
    void* ret=cuckoo_remove(c->c, key);
    pthread_mutex_unlock(&c->mutex);
    return ret;
}

void *cuckoo_mutex_get(struct cs *c, uint64_t key)
{
    pthread_mutex_lock(&c->mutex);
    void* ret=cuckoo_get(c->c, key);
    pthread_mutex_unlock(&c->mutex);
    return ret;
}

size_t cuckoo_mutex_get_size(struct cs *c)
{
    pthread_mutex_lock(&c->mutex);
    size_t ret=cuckoo_get_size(c->c);
    pthread_mutex_unlock(&c->mutex);
    return ret;
}

void cuckoo_mutex_get_stats(void *c, uint64_t *buf, int nstat)
{
}

uint64_t cuckoo_mutex_debug(struct cs *c, uint64_t arg)
{
    return 0;
}
