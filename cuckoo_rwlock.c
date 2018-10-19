#include <pthread.h>
#include <stdlib.h>

#include "cuckoo.h"
#include "util.h"

struct cs
{
    struct cuckoo *c;
    volatile uint64_t lock;
    pthread_mutex_t mutex;
};

/*
 * The lock consist of two 32-bit fields, plus a mutex.
 * Lower field is a count of fast readers, upper of writers.
 * A slow reader (ie, one who noticed a writer) takes the mutex,
 * so does any writer.
 */

#define ANY_READERS 0x00000000ffffffff
#define ANY_WRITERS 0xffffffff00000000
#define READER      0x0000000000000001
#define WRITER      0x0000000100000000

static inline int rwlock_read_enter(struct cs *c)
{
    if (util_fetch_and_add64(&c->lock, 1) & ANY_WRITERS)
    {
        util_fetch_and_sub64(&c->lock, 1);
        pthread_mutex_lock(&c->mutex);
        return 1;
    }
    return 0;
}

static inline void rwlock_read_leave(struct cs *c, int busy)
{
    if (busy)
        pthread_mutex_unlock(&c->mutex);
    else
        util_fetch_and_sub64(&c->lock, 1);
}

static inline void rwlock_write_enter(struct cs *c)
{
    util_fetch_and_add64(&c->lock, WRITER); /* bare add would be enough */
    while (c->lock & ANY_READERS)
            ; /* let fast readers go away */
    pthread_mutex_lock(&c->mutex);
}

static inline void rwlock_write_leave(struct cs *c)
{
    util_fetch_and_sub64(&c->lock, WRITER);
    pthread_mutex_unlock(&c->mutex);
}

struct cs *cuckoo_rwlock_new(void)
{
    struct cs *c = malloc(sizeof(struct cs));
    c->c=cuckoo_new();
    c->lock=0;
    pthread_mutex_init(&c->mutex, 0);
    return c;
}

void cuckoo_rwlock_delete(struct cs *c)
{
    pthread_mutex_destroy(&c->mutex);
    cuckoo_delete(c->c);
}

int cuckoo_rwlock_insert(struct cs *c, uint64_t key, void *value)
{
    rwlock_write_enter(c);
    int ret=cuckoo_insert(c->c, key, value);
    rwlock_write_leave(c);
    return ret;
}

void *cuckoo_rwlock_remove(struct cs *c, uint64_t key)
{
    rwlock_write_enter(c);
    void* ret=cuckoo_remove(c->c, key);
    rwlock_write_leave(c);
    return ret;
}

void *cuckoo_rwlock_get(struct cs *c, uint64_t key)
{
    int busy = rwlock_read_enter(c);
    void* ret=cuckoo_get(c->c, key);
    rwlock_read_leave(c, busy);
    return ret;
}

size_t cuckoo_rwlock_get_size(struct cs *c)
{
    int busy = rwlock_read_enter(c);
    size_t ret=cuckoo_get_size(c->c);
    rwlock_read_leave(c, busy);
    return ret;
}

void cuckoo_rwlock_get_stats(void *c, uint64_t *buf, int nstat)
{
}

uint64_t cuckoo_rwlock_debug(struct cs *c, uint64_t arg)
{
    return 0;
}
