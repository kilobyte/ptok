#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "hmproto.h"

#define ARRAYSZ(x) (sizeof(x)/sizeof(x[0]))
#define NTHREADS 8

static uint64_t rnd16()
{
    return rand()&0xffff;
}

static uint64_t rnd64()
{
    return rnd16()<<48 | rnd16()<<32 | rnd16()<<16 | rnd16();
}

static int bad=0;
#define CHECK(x) do if (!(x)) bad=1; while (0)
static int done=0;

static uint64_t the1000[1000];

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
    for (int i=0; i<999; i++)
        hm_insert(c, the1000[i], (void*)the1000[i]);

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

static void test_read1000_of_1000()
{
    void *c = hm_new();
    for (int i=0; i<1000; i++)
        hm_insert(c, the1000[i], (void*)the1000[i]);

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

static void* thread_write_1000(void* c)
{
    uint64_t count=0;
    int i=0;
    while (!done)
    {
        if (++i==1000)
            i=0;
        uint64_t v=the1000[i];
        hm_insert(c, v, (void*)v);
        count++;
    }
    return (void*)count;
}

static void test_read1_write_1000()
{
    void *c = hm_new();
    hm_insert(c, K, (void*)K);

    pthread_t th[NTHREADS], wr[NTHREADS];
    done=0;
    for (int i=0; i<NTHREADS; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1, c));
    for (int i=0; i<NTHREADS; i++)
        CHECK(!pthread_create(&wr[i], 0, thread_write_1000, c));
    sleep(1);
    done=1;

    uint64_t countr=0, countw=0;
    for (int i=0; i<NTHREADS; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        countr+=(uintptr_t)retval;
    }
    for (int i=0; i<NTHREADS; i++)
    {
        void* retval;
        CHECK(!pthread_join(wr[i], &retval));
        countw+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu %15lu\n", countr, countw);
}

static void test_read1000_write_1000()
{
    void *c = hm_new();
    for (int i=0; i<1000; i++)
        hm_insert(c, the1000[i], (void*)the1000[i]);

    pthread_t th[NTHREADS], wr[NTHREADS];
    done=0;
    for (int i=0; i<NTHREADS; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1000_of_1000, c));
    for (int i=0; i<NTHREADS; i++)
        CHECK(!pthread_create(&wr[i], 0, thread_write_1000, c));
    sleep(1);
    done=1;

    uint64_t countr=0, countw=0;
    for (int i=0; i<NTHREADS; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        countr+=(uintptr_t)retval;
    }
    for (int i=0; i<NTHREADS; i++)
    {
        void* retval;
        CHECK(!pthread_join(wr[i], &retval));
        countw+=(uintptr_t)retval;
    }

    hm_delete(c);
    printf("\e[F\e[40C%15lu %15lu\n", countr, countw);
}

static void* thread_read_write_remove(void* c)
{
    uint64_t count=0;
    while (!done)
    {
        uint64_t v=rnd64();
        hm_insert(c, v, (void*)v);
        CHECK(hm_get(c, v) == (void*)v);
        CHECK(hm_remove(c, v) == (void*)v);
        count++;
    }
    return (void*)count;
}

static void test_read_write_remove()
{
    void *c = hm_new();

    pthread_t th[NTHREADS];
    done=0;
    for (int i=0; i<NTHREADS; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read_write_remove, c));
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
            printf("\e[F \e[31m[\e[1m✗\e[22m]\e[0m\n");
    }
}
#define TEST(x,mut) do run_test(test_##x, #x, mut); while (0)

int main()
{
    for (int i=0; i<ARRAYSZ(the1000); i++)
        the1000[i] = rnd64();

    printf("Using %d threads.\n", NTHREADS);
    TEST(read1, 0);
    TEST(read1_of_2, 0);
    TEST(read1_of_1000, 0);
    TEST(read1000_of_1000, 0);
    TEST(read1_write_1000, 1);
    TEST(read1000_write_1000, 1);
    TEST(read_write_remove, 1);
    return 0;
}
