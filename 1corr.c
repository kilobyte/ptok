#include <stdio.h>
#include <stdlib.h>
#include "hmproto.h"

#define ARRAYSZ(x) (sizeof(x)/sizeof(x[0]))

static int bad=0;
#define CHECK(x) do if (!(x)) printf("\e[31mWRONG: \e[1m%s\e[22m at line \e[1m%d\e[22m\n", #x, __LINE__),bad=1,exit(1); while (0)

static void test_smoke()
{
    void *c = hm_new();
    hm_insert(c, 123, (void*)456);
    CHECK(hm_get(c, 123) == (void*)456);
    CHECK(hm_get(c, 124) == 0);
    hm_delete(c);
}

static void test_1to1000()
{
    void *c = hm_new();
    for (long i=0; i<1000; i++)
        hm_insert(c, i, (void*)i);
    for (long i=0; i<1000; i++)
        CHECK(hm_get(c, i) == (void*)i);
    hm_delete(c);
}

static void test_insert_delete32M()
{
    #define MAX (32*1048576)
    void *c = hm_new();
    for (long i=0; i<MAX; i++)
    {
        CHECK(hm_get(c, i) == (void*)0);
        hm_insert(c, i, (void*)i);
        CHECK(hm_get(c, i) == (void*)i);
        CHECK(hm_remove(c, i) == (void*)i);
        CHECK(hm_get(c, i) == (void*)0);
    }
    hm_delete(c);
    #undef MAX
}

static void test_ffffffff_and_friends()
{
    static uint64_t vals[]=
    {
        0,
        0x7fffffff,
        0x80000000,
        0xffffffff,
        0x7fffffffFFFFFFFF,
        0x8000000000000000,
        0xFfffffffFFFFFFFF,
    };

    void *c = hm_new();
    for (int i=0; i<ARRAYSZ(vals); i++)
        hm_insert(c, vals[i], (void*)~vals[i]);
    for (int i=0; i<ARRAYSZ(vals); i++)
        CHECK(hm_get(c, vals[i]) == (void*)~vals[i]);
    hm_delete(c);
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
    HM_SELECT(cuckoo_mutex);
    TEST(smoke);
    TEST(1to1000);
    TEST(insert_delete32M);
    TEST(ffffffff_and_friends);
    return 0;
}
