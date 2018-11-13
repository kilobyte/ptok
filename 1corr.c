#include <stdio.h>
#include <stdlib.h>
#include "hmproto.h"

#define ARRAYSZ(x) (sizeof(x)/sizeof(x[0]))

static int bad=0;
#define CHECK(x) do if (!(x)) printf("\e[31mWRONG: \e[1m%s\e[22m at line \e[1m%d\e[22m\n", #x, __LINE__),bad=1,exit(1); while (0)

static uint64_t rnd64()
{
    return (uint64_t)(uint32_t)(mrand48())<<32 | (uint32_t)(mrand48());
}

static void test_smoke()
{
    void *c = hm_new();
    hm_insert(c, 123, (void*)456);
    CHECK(hm_get(c, 123) == (void*)456);
    CHECK(hm_get(c, 124) == 0);
    hm_delete(c);
}

static void test_key0()
{
    void *c = hm_new();
    hm_insert(c, 1, (void*)1);
    hm_insert(c, 0, (void*)2);
    hm_insert(c, 65536, (void*)3);
    CHECK(hm_get(c, 1)    == (void*)1);
    CHECK(hm_remove(c, 1) == (void*)1);
    CHECK(hm_get(c, 0)      == (void*)2);
    CHECK(hm_remove(c, 0)   == (void*)2);
    CHECK(hm_get(c, 65536)    == (void*)3);
    CHECK(hm_remove(c, 65536) == (void*)3);
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

static void test_insert_delete1M()
{
    #define MAX 1048576
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

static void test_insert_bulk_delete1M()
{
    #define MAX (1048576)
    void *c = hm_new();
    for (long i=0; i<MAX; i++)
    {
        CHECK(hm_get(c, i) == (void*)0);
        hm_insert(c, i, (void*)i);
        CHECK(hm_get(c, i) == (void*)i);
    }
    for (long i=0; i<MAX; i++)
    {
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
    for (int i=0; i<ARRAYSZ(vals); i++)
        CHECK(hm_remove(c, vals[i]) == (void*)~vals[i]);
    hm_delete(c);
}

static void test_insert_delete_random()
{
    void *c = hm_new();
    for (long i=0; i<1000000; i++)
    {
        uint64_t v=rnd64();
        hm_insert(c, v, (void*)v);
        CHECK(hm_get(c, v) == (void*)v);
        CHECK(hm_remove(c, v) == (void*)v);
        CHECK(hm_get(c, v) == 0);
    }
    hm_delete(c);
}

static void test_le_basic()
{
    void *c = hm_new();
#define INS(x) hm_insert(c, (x), (void*)(x))
    INS(1);
    INS(2);
    INS(3);
    INS(0);
    INS(4);
    INS(0xf);
    INS(0xe);
    INS(0x11);
    INS(0x12);
    INS(0x20);
#define GET_SAME(x) CHECK(hm_get(c, (x)) == (void*)(x))
#define GET_NULL(x) CHECK(hm_get(c, (x)) == NULL)
    GET_NULL(122);
    GET_SAME(1);
    GET_SAME(2);
    GET_SAME(3);
    GET_SAME(4);
    GET_NULL(5);
    GET_SAME(0x11);
    GET_SAME(0x12);
#define LE(x,y) CHECK(hm_find_le(c, (x)) == (void*)(y))
    LE(1, 1);
    LE(2, 2);
    LE(5, 4);
    LE(6, 4);
    LE(0x11, 0x11);
    LE(0x15, 0x12);
    LE(0xfffffff, 0x20);
    hm_delete(c);
}

static void run_test(void (*func)(void), const char *name, int req)
{
    printf("TEST: %s\n", name);
    for (int i=0; i<ARRAYSZ(hms); i++)
    {
        hm_select(i);
        if (hm_immutable & req)
        {
            printf(" \e[35m[\e[1m!\e[22m]\e[0m: %s\n", hm_name);
            continue;
        }
        printf(" \e[34m[\e[1m⚒\e[22m]\e[0m: %s\n", hm_name);
        bad=0;
        func();
        if (!bad)
            printf("\e[F \e[32m[\e[1m✓\e[22m]\e[0m\n");
    }
}
#define TEST(x,req) do run_test(test_##x, #x, req); while (0)

int main()
{
    TEST(smoke, 0);
    TEST(key0, 0);
    TEST(1to1000, 0);
    TEST(insert_delete1M, 0);
    TEST(insert_bulk_delete1M, 0);
    TEST(ffffffff_and_friends, 0);
    TEST(insert_delete_random, 0);
    TEST(le_basic, 2);
    return 0;
}
