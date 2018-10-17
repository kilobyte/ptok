#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "hmproto.h"
#include "tlog.h"

#define ARRAYSZ(x) (sizeof(x)/sizeof(x[0]))

// These functions return a 64-bit value on 64-bit systems; I'd be
// interested in testing stuff on a box where you have more cores
// than fits in an uint32_t...
static uint64_t nthreads, nrthreads, nwthreads;

static uint64_t nproc()
{
    cpu_set_t set;
    if (!pthread_getaffinity_np(pthread_self(), sizeof(set), &set))
        return CPU_COUNT(&set);
    return 0;
}

static uint64_t rnd16()
{
    return rand()&0xffff;
}

static uint64_t rnd64()
{
    return rnd16()<<48 | rnd16()<<32 | rnd16()<<16 | rnd16();
}

static uint64_t rnd_r64(unsigned short xsubi[3])
{
    return (uint64_t)(uint32_t)(jrand48(xsubi))<<32 | (uint32_t)(jrand48(xsubi));
}

static int bad=0, any_bad=0;
#define CHECK(x) do if (!(x)) bad=1; while (0)
#define CHECKP(x,...) do if (!(x)) {bad=1; printf("ERROR: "__VA_ARGS__);} while (0)
static int done=0;

static uint64_t the1000[1000];

#define K 0xdeadbeefcafebabe

/***********/
/* threads */
/***********/

static void* thread_read1(void* c)
{
    uint64_t count=0;
    while (!done)
    {
        CHECK(hm_get(c, K) == (void*)K);
        count++;
    }
    return (void*)count;
}

static void* thread_read1000_of_1000(void* c)
{
    uint64_t count=0;
    int i=0;
    while (!done)
    {
        if (++i==1000)
            i=0;
        uint64_t v=the1000[i];
        CHECK(hm_get(c, v) == (void*)v);
        count++;
    }
    return (void*)count;
}

static void* thread_write1000(void* c)
{
    unsigned short xsubi[3];
    xsubi[0]=pthread_self()>>32;
    xsubi[1]=pthread_self()>>16;
    xsubi[2]=pthread_self();
    uint64_t w1000[1000];
    for (int i=0; i<ARRAYSZ(w1000); i++)
        w1000[i] = rnd_r64(xsubi);

    uint64_t count=0;
    int i=0;
    while (!done)
    {
        if (++i==1000)
            i=0;
        uint64_t v=w1000[i];
        hm_insert(c, v, (void*)v);
        uint64_t r=(uint64_t)hm_remove(c, v);
        CHECK(v==r);
        count++;
    }
    return (void*)count;
}

static void* thread_read_write_remove(void* c)
{
    unsigned short xsubi[3];
    getentropy(xsubi, sizeof(xsubi));
    uint64_t count=0;
    while (!done)
    {
        uint64_t r, v=rnd_r64(xsubi);
        hm_insert(c, v, (void*)v);
        r = (uint64_t)hm_get(c, v);
        CHECKP(r == v, "get[%016lx] got %016lx\n\n", v, r);
        r = (uint64_t)hm_remove(c, v);
        CHECKP(r == v, "remove[%016lx] got %016lx\n\n", v, r);
        count++;
    }
    return (void*)count;
}

#define CACHESIZE 512 /* 32KB in 64-byte cachelines */

static void* thread_read1_cachekiller(void* c)
{
    volatile uint64_t cache[CACHESIZE][8];

    uint64_t count=0;
    while (!done)
    {
        for (int i=0; i<CACHESIZE; i++)
            cache[i][0]++;
        CHECK(hm_get(c, K) == (void*)K);
        count++;
    }
    return (void*)count;
}

static void* thread_write1000_cachekiller(void* c)
{
    volatile uint64_t cache[CACHESIZE][8];
    unsigned short xsubi[3];
    xsubi[0]=pthread_self()>>32;
    xsubi[1]=pthread_self()>>16;
    xsubi[2]=pthread_self();
    uint64_t w1000[1000];
    for (int i=0; i<ARRAYSZ(w1000); i++)
        w1000[i] = rnd_r64(xsubi);

    uint64_t count=0;
    int i=0;
    while (!done)
    {
        for (int k=0; k<CACHESIZE; k++)
            cache[k][0]++;
        if (++i==1000)
            i=0;
        uint64_t v=w1000[i];
        hm_insert(c, v, (void*)v);
        uint64_t r=(uint64_t)hm_remove(c, v);
        CHECK(v==r);
        count++;
    }
    return (void*)count;
}

/*********/
/* tests */
/*********/

static void test_read1()
{
    void *c = hm_new();
    hm_insert(c, K, (void*)K);

    pthread_t th[nthreads];
    done=0;
    for (int i=0; i<nthreads; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1, c));
    sleep(1);
    done=1;

    uint64_t count=0;
    for (int i=0; i<nthreads; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        count+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu\n", count);
}

static void test_read1_of_2()
{
    void *c = hm_new();
    hm_insert(c, K, (void*)K);
    hm_insert(c, 1, (void*)1);

    pthread_t th[nthreads];
    done=0;
    for (int i=0; i<nthreads; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1, c));
    sleep(1);
    done=1;

    uint64_t count=0;
    for (int i=0; i<nthreads; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        count+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu\n", count);
}

static void test_read1_of_1000()
{
    void *c = hm_new();
    hm_insert(c, K, (void*)K);
    for (int i=0; i<999; i++)
        hm_insert(c, the1000[i], (void*)the1000[i]);

    pthread_t th[nthreads];
    done=0;
    for (int i=0; i<nthreads; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1, c));
    sleep(1);
    done=1;

    uint64_t count=0;
    for (int i=0; i<nthreads; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        count+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu\n", count);
}

static void test_read1000_of_1000()
{
    void *c = hm_new();
    for (int i=0; i<1000; i++)
        hm_insert(c, the1000[i], (void*)the1000[i]);

    pthread_t th[nthreads];
    done=0;
    for (int i=0; i<nthreads; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1000_of_1000, c));
    sleep(1);
    done=1;

    uint64_t count=0;
    for (int i=0; i<nthreads; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        count+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu\n", count);
}

static void test_read1_write1000()
{
    void *c = hm_new();
    hm_insert(c, K, (void*)K);

    pthread_t th[nrthreads], wr[nwthreads];
    done=0;
    for (int i=0; i<nrthreads; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1, c));
    for (int i=0; i<nwthreads; i++)
        CHECK(!pthread_create(&wr[i], 0, thread_write1000, c));
    sleep(1);
    done=1;

    uint64_t countr=0, countw=0;
    for (int i=0; i<nrthreads; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        countr+=(uintptr_t)retval;
    }
    for (int i=0; i<nwthreads; i++)
    {
        void* retval;
        CHECK(!pthread_join(wr[i], &retval));
        countw+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu %15lu\n", countr, countw);
}

static void test_read1000_write1000()
{
    void *c = hm_new();
    for (int i=0; i<1000; i++)
        hm_insert(c, the1000[i], (void*)the1000[i]);

    pthread_t th[nrthreads], wr[nwthreads];
    done=0;
    for (int i=0; i<nrthreads; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1000_of_1000, c));
    for (int i=0; i<nwthreads; i++)
        CHECK(!pthread_create(&wr[i], 0, thread_write1000, c));
    sleep(1);
    done=1;

    uint64_t countr=0, countw=0;
    for (int i=0; i<nrthreads; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        countr+=(uintptr_t)retval;
    }
    for (int i=0; i<nwthreads; i++)
    {
        void* retval;
        CHECK(!pthread_join(wr[i], &retval));
        countw+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu %15lu\n", countr, countw);
}

static void test_read_write_remove()
{
    void *c = hm_new();

    pthread_t th[nthreads];
    done=0;
    for (int i=0; i<nthreads; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read_write_remove, c));
    sleep(1);
    done=1;

    uint64_t count=0;
    for (int i=0; i<nthreads; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        count+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu\n", count);
}

static void test_read1_cachekiller()
{
    void *c = hm_new();
    hm_insert(c, K, (void*)K);

    pthread_t th[nthreads];
    done=0;
    for (int i=0; i<nthreads; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1_cachekiller, c));
    sleep(1);
    done=1;

    uint64_t count=0;
    for (int i=0; i<nthreads; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        count+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu\n", count);
}

static void* thread_read1000_of_1000_cachekiller(void* c)
{
    volatile uint64_t cache[CACHESIZE][8];

    uint64_t count=0;
    int i=0;
    while (!done)
    {
        for (int k=0; k<CACHESIZE; k++)
            cache[k][0]++;
        if (++i==1000)
            i=0;
        uint64_t v=the1000[i];
        CHECK(hm_get(c, v) == (void*)v);
        count++;
    }
    return (void*)count;
}

static void test_read1000_of_1000_cachekiller()
{
    void *c = hm_new();
    for (int i=0; i<1000; i++)
        hm_insert(c, the1000[i], (void*)the1000[i]);

    pthread_t th[nthreads];
    done=0;
    for (int i=0; i<nthreads; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1000_of_1000_cachekiller, c));
    sleep(1);
    done=1;

    uint64_t count=0;
    for (int i=0; i<nthreads; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        count+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu\n", count);
}

static void test_read1000_write1000_cachekiller()
{
    void *c = hm_new();
    for (int i=0; i<1000; i++)
        hm_insert(c, the1000[i], (void*)the1000[i]);

    pthread_t th[nrthreads], wr[nwthreads];
    done=0;
    for (int i=0; i<nrthreads; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1000_of_1000_cachekiller, c));
    for (int i=0; i<nwthreads; i++)
        CHECK(!pthread_create(&wr[i], 0, thread_write1000_cachekiller, c));
    sleep(1);
    done=1;

    uint64_t countr=0, countw=0;
    for (int i=0; i<nrthreads; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        countr+=(uintptr_t)retval;
    }
    for (int i=0; i<nwthreads; i++)
    {
        void* retval;
        CHECK(!pthread_join(wr[i], &retval));
        countw+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu %15lu\n", countr, countw);
}

static void run_test(void (*func)(void), const char *name, int mut)
{
    printf("TEST: %s\n", name);

    for (int i=0; i<ARRAYSZ(hms); i++)
    {
        hm_select(i);
        if (mut && hm_immutable)
        {
            printf(" \e[35m[\e[1m!\e[22m]\e[0m: %s\n", hm_name);
            continue;
        }

        printf(" \e[34m[\e[1m⚒\e[22m]\e[0m: %s\n", hm_name);
        bad=0;
        func();
        if (!bad)
            printf("\e[F \e[32m[\e[1m✓\e[22m]\e[0m\n");
        else
            printf("\e[F \e[31m[\e[1m✗\e[22m]\e[0m\n"), any_bad=1;
    }
}
#define TEST(x,mut) do run_test(test_##x, #x, mut); while (0)

int main()
{
    for (int i=0; i<ARRAYSZ(the1000); i++)
        the1000[i] = rnd64();

    nthreads = nproc();
    if (!nthreads)
        nthreads = 8;
    nwthreads=nthreads/2;
    if (!nwthreads)
        nwthreads = 1;
    nrthreads=nthreads-nwthreads;
    if (!nrthreads)
        nrthreads = 1;
    printf("Using %lu threads; %lu readers %lu writers in mixed tests.\n",
        nthreads, nrthreads, nwthreads);
    TEST(read1, 0);
    TEST(read1_of_2, 0);
    TEST(read1_of_1000, 0);
    TEST(read1000_of_1000, 0);
    TEST(read1_write1000, 1);
    TEST(read1000_write1000, 1);
    TEST(read_write_remove, 1);
    TEST(read1_cachekiller, 0);
    TEST(read1000_of_1000_cachekiller, 0);
    TEST(read1000_write1000_cachekiller, 1);
    return any_bad;
}
