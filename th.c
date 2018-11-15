#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
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

static void randomize(unsigned short *xsubi)
{
    if (syscall(SYS_getrandom, xsubi, sizeof(xsubi), 0) == sizeof(xsubi))
        return;
    xsubi[0]=pthread_self()>>32;
    xsubi[1]=pthread_self()>>16;
    xsubi[2]=pthread_self();
}

static int bad=0, any_bad=0;
#define CHECK(x) do if (!(x)) bad=1; while (0)
#define CHECKP(x,...) do if (!(x)) {bad=1; printf("ERROR: "__VA_ARGS__);} while (0)
static int done=0;

static uint64_t the1000[1000];
static void* the1000p[1000];

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

static void* thread_read1p(void* c)
{
    void* k = the1000p[0];

    uint64_t count=0;
    while (!done)
    {
        CHECK(hm_get(c, (uint64_t)k) == k);
        count++;
    }
    return (void*)count;
}

static void* thread_read1000(void* c)
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
    randomize(xsubi);
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
    randomize(xsubi);
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

static void* thread_read1000_cachekiller(void* c)
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

static uint64_t revbits(uint64_t x)
{
    uint64_t y=0, a=1, b=0x8000000000000000;
    for (; b; a<<=1, b>>=1)
    {
        if (x & a)
            y |= b;
    }
    return y;
}

static void* thread_le1(void* c)
{
    uint64_t count=0;
    while (!done)
    {
        uint64_t y = revbits(count);
        if (y < K)
            CHECK(hm_find_le(c, y) == NULL);
        else
            CHECK(hm_find_le(c, y) == (void*)K);
        count++;
    }
    return (void*)count;
}

/*********/
/* tests */
/*********/

typedef void *(*thread_func_t)(void *);

static void run_test(int spreload, int rpreload, thread_func_t rthread, thread_func_t wthread)
{
    int ptrs = (rpreload<0);

    void *c = hm_new();
    if (spreload>=1)
        hm_insert(c, K, (void*)K);
    if (spreload>=2)
        hm_insert(c, 1, (void*)1);
    if (ptrs)
    {
        rpreload=-rpreload;
        for (int i=spreload; i<rpreload; i++)
            hm_insert(c, (uint64_t)the1000p[i], the1000p[i]);
    }
    else
        for (int i=spreload; i<rpreload; i++)
            hm_insert(c, the1000[i], (void*)the1000[i]);

    pthread_t th[nthreads], wr[nwthreads];
    int ntr=wthread?nrthreads:nthreads;
    int ntw=wthread?nwthreads:0;
    done=0;
    for (int i=0; i<ntr; i++)
        CHECK(!pthread_create(&th[i], 0, rthread, c));
    for (int i=0; i<ntw; i++)
        CHECK(!pthread_create(&wr[i], 0, wthread, c));
    sleep(1);
    done=1;

    uint64_t countr=0, countw=0;
    for (int i=0; i<ntr; i++)
    {
        void* retval;
        CHECK(!pthread_join(th[i], &retval));
        countr+=(uintptr_t)retval;
    }
    for (int i=0; i<ntw; i++)
    {
        void* retval;
        CHECK(!pthread_join(wr[i], &retval));
        countw+=(uintptr_t)retval;
    }

#ifdef TRACEMEM
    uint64_t mem = hm_get_size(c);
    uint64_t stats[3] = {0,0,0};
    hm_get_stats(c, stats, ARRAYSZ(stats));
    printf("\e[F\e[25Cmem:%10ld avg.depth: %5.2f\n", mem, stats[2]?(double)stats[1]/stats[2]:0);
#else
    if (ntw)
        printf("\e[F\e[25C%15lu %15lu\n", countr, countw);
    else
        printf("\e[F\e[25C%15lu\n", countr);
#endif
    hm_delete(c);
}

static int only_hm = -1;

static void test(const char *name, int spreload, int rpreload,
    thread_func_t rthread, thread_func_t wthread, int req)
{
    printf("TEST: %s\n", name);

    int hmin = (only_hm<0)?0:only_hm;
    int hmax = (only_hm>=0 && only_hm<ARRAYSZ(hms))?only_hm:ARRAYSZ(hms)-1;
    for (int i=hmin; i<=hmax; i++)
    {
        hm_select(i);
        if ((wthread && (hm_immutable&1)) || hm_immutable&req)
        {
            printf(" \e[35m[\e[1m!\e[22m]\e[0m: %s\n", hm_name);
            continue;
        }

        printf(" \e[34m[\e[1m⚒\e[22m]\e[0m: %s\e[0m\n", hm_name);
        bad=0;
        run_test(spreload, rpreload, rthread, ((intptr_t)wthread==-1)?0:wthread);
        if (!bad)
            printf("\e[F \e[32m[\e[1m✓\e[22m]\e[0m\n");
        else
            printf("\e[F \e[31m[\e[1m✗\e[22m]\e[0m\n"), any_bad=1;
    }
}

int main(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "a:")) != -1)
    {
        switch (opt)
        {
        case 'a':
            only_hm = atoi(optarg);
            break;
        default:
            exit(1);
        }
    }
    if (optind < argc)
        return fprintf(stderr, "%s: unknown arg '%s'\n", argv[0], argv[optind]), 1;

    for (int i=0; i<ARRAYSZ(the1000); i++)
        the1000[i] = rnd64();
    for (int i=0; i<ARRAYSZ(the1000p); i++)
        the1000p[i] = malloc(the1000[i]%65536+1);

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
    test("read 1-of-1", 1, 0, thread_read1, 0, 0);
    test("read 1-of-2", 2, 0, thread_read1, 0, 0);
    test("read 1-of-1000", 1, 1000, thread_read1, 0, 0);
    test("read 1000-of-1000", 0, 1000, thread_read1000, 0, 0);
    test("read 1-of-1000 pointers", 0, -1000, thread_read1p, 0, 0);
    test("read 1 write 1000", 1, 0, thread_read1, thread_write1000, 0);
    test("read 1000 write 1000", 0, 1000, thread_read1000, thread_write1000, 0);
    test("read-write-remove", 0, 0, thread_read_write_remove, (thread_func_t)-1, 0);
    test("read 1-of-1 cachekiller", 1, 0, thread_read1_cachekiller, 0, 0);
    test("read 1-of-1000 cachekiller", 1, 1000, thread_read1_cachekiller, 0, 0);
    test("read 1000 write 1000 cachekiller", 0, 1000, thread_read1000_cachekiller, thread_write1000_cachekiller, 0);
    test("le 1 van der Corput", 1, 0, thread_le1, 0, 2);

    for (int i=0; i<ARRAYSZ(the1000p); i++)
        free(the1000p[i]);
    return any_bad;
}
