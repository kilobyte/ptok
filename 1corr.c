#include <stdio.h>
#include "hmproto.h"

static int bad=0;
#define CHECK(x) do if (!(x)) printf("\e[31mWRONG: \e[1m%s\e[22m at line \e[1m%d\e[22m\n", #x, __LINE__),bad=1; while (0)

static void test_smoke()
{
    void *c = hm_new();
    hm_insert(c, 123, (void*)456);
    CHECK(hm_get(c, 123) == (void*)456);
    CHECK(hm_get(c, 124) == 0);
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
    HM_SELECT(cuckoo);
    TEST(smoke);
    return 0;
}
