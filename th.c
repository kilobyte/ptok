#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "hmproto.h"

#define ARRAYSZ(x) (sizeof(x)/sizeof(x[0]))
#define NTHREADS 8

static uint64_t rnd16(unsigned int *seedp)
{
    return rand_r(seedp)&0xffff;
}

static uint64_t rnd64(unsigned int *seedp)
{
    return rnd16(seedp)<<48 | rnd16(seedp)<<32 | rnd16(seedp)<<16 | rnd16(seedp);
}

static int bad=0;
#define CHECK(x) do if (!(x)) bad=1; while (0)
static int done=0;


#define K 0xdeadbeefcafebabe

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

static void test_read1()
{
    void *c = hm_new();
    hm_insert(c, K, (void*)K);

    pthread_t th[NTHREADS];
    done=0;
    for (int i=0; i<NTHREADS; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1, c));
    sleep(1);
    done=1;

    uint64_t count=0;
    for (int i=0; i<NTHREADS; i++)
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

    pthread_t th[NTHREADS];
    done=0;
    for (int i=0; i<NTHREADS; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1, c));
    sleep(1);
    done=1;

    uint64_t count=0;
    for (int i=0; i<NTHREADS; i++)
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
    unsigned int seed=0;
    for (int i=0; i<999; i++)
    {
        uint64_t v = rnd64(&seed);
        hm_insert(c, v, (void*)v);
    }

    pthread_t th[NTHREADS];
    done=0;
    for (int i=0; i<NTHREADS; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1, c));
    sleep(1);
    done=1;

    uint64_t count=0;
    for (int i=0; i<NTHREADS; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        count+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu\n", count);
}

static void* thread_read1000_of_1000(void* c)
{
    uint64_t count=0;
    unsigned int seed=0;
    while (!done)
    {
        uint64_t v=rnd64(&seed);
        CHECK(hm_get(c, v) == (void*)v);
        if (!(++count%1000))
            seed=0;
    }
    return (void*)count;
}

static void test_read1000_of_1000()
{
    void *c = hm_new();
    unsigned int seed=0;
    for (int i=0; i<1000; i++)
    {
        uint64_t v=rnd64(&seed);
        hm_insert(c, v, (void*)v);
    }

    pthread_t th[NTHREADS];
    done=0;
    for (int i=0; i<NTHREADS; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1000_of_1000, c));
    sleep(1);
    done=1;

    uint64_t count=0;
    for (int i=0; i<NTHREADS; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        count+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu\n", count);
}

static void run_test(void (*func)(void), const char *name)
{
    printf("TEST: %s\n", name);
    bad=0;
    func();
    if (!bad)
        printf("\e[F \e[32m[\e[1m✓\e[22m]\e[0m\n");
    else
        printf("\e[F \e[31m[\e[1m✗\e[22m]\e[0m\n");
}
#define TEST(x) do run_test(test_##x, #x); while (0)

int main()
{
    printf("Using %d threads.\n", NTHREADS);
    HM_SELECT(cuckoo);
    printf("%s\n", hm_name);
    TEST(read1);
    TEST(read1_of_2);
    TEST(read1_of_1000);
    TEST(read1000_of_1000);
    HM_SELECT(cuckoo_mutex);
    printf("%s\n", hm_name);
    TEST(read1);
    TEST(read1_of_2);
    TEST(read1_of_1000);
    TEST(read1000_of_1000);
    return 0;
}
