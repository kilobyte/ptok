#include <stdio.h>
#include "hmproto.h"

int main()
{
    HM_SELECT(critnib_tag);
    void *c = hm_new();
    hm_insert(c, 0x1, "1");
    hm_insert(c, 0x0, "0");
    hm_insert(c, 0x2, "2");
#define GET(x) printf("get(%lx) → %s\n", (uint64_t)(x), (const char*)hm_get(c, (x)))
    GET(122);
    GET(0);
    GET(1);
    GET(2);
    GET(3);
    GET(4);
    GET(5);
    GET(0x11);
    GET(0x12);
#define LE(x) printf("le(%lx) → %s\n", (uint64_t)(x), (const char*)hm_find_le(c, (x)))
    LE(1);
    LE(2);
    LE(5);
    LE(6);
    LE(0x11);
    LE(0x15);
    LE(0xfffffff);

    hm_delete(c);
    return 0;
}
