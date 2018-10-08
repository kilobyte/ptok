#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "hmproto.h"

#define ARRAYSZ(x) (sizeof(x)/sizeof(x[0]))
#define NTHREADS 8

static int bad=0;
#define CHECK(x) do if (!(x)) printf("\e[31mWRONG: \e[1m%s\e[22m at line \e[1m%d\e[22m\n", #x, __LINE__),bad=1; while (0)
static int done=0;


#define K 0xdeadbeefcafebabe
#define V (void*)0x0123456789ABCDEF

static void* thread_read1(void* c)
{
    uint64_t count=0;
    while (!done)
    {
        CHECK(hm_get(c, K) == V);
        count++;
    }
    return (void*)count;
}

static void test_read1()
{
    void *c = hm_new();
    hm_insert(c, K, V);

    pthread_t th[NTHREADS];
    for (int i=0; i<NTHREADS; i++)
        CHECK(!pthread_create(&th[i], 0, thread_read1, c));
    sleep(1);
    done=1;

    uint64_t count;
    for (int i=0; i<NTHREADS; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        count+=(uintptr_t)retval;
    }
    
    hm_delete(c);
    printf("\e[F\e[40C%lu\n", count);
}

static void run_test(void (*func)(void), const char *name)
{
    printf("TEST: %s\n", name);
    bad=0;
    func();
    if (!bad)
        printf("\e[F \e[32m[\e[1m✓\e[22m]\e[0m\n");
}
#define TEST(x) do run_test(test_##x, #x); while (0)

int main()
{
    HM_SELECT(cuckoo);
    printf("Using %d threads.\n", NTHREADS);
    TEST(read1);
    return 0;
}
