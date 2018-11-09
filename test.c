#include <stdio.h>
#include "hmproto.h"

int main()
{
    HM_SELECT(critnib);
    void *c = hm_new();
    hm_insert(c, 0x1, "1");
    hm_insert(c, 0x2, "2");
    hm_insert(c, 0x3, "3");
    hm_insert(c, 0x0, "0");
    hm_insert(c, 0x4, "4");
    hm_insert(c, 0xf, "f");
    hm_insert(c, 0xe, "e");
    hm_insert(c, 0x11, "11");
    hm_insert(c, 0x12, "12");
    hm_insert(c, 0x20, "20");
#define GET(x) printf("%lx = %s\n", (uint64_t)(x), (const char*)hm_get(c, (x)))
    GET(122);
    GET(1);
    GET(2);
    GET(3);
    GET(4);
    GET(5);
    GET(0x11);
    GET(0x12);
#define LE(x) printf("%lx <= %s\n", (uint64_t)(x), (const char*)hm_find_le(c, (x)))
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
