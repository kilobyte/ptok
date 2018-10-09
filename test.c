#include <stdio.h>
#include "hmproto.h"

int main()
{
    HM_SELECT(tcradix);
    void *c = hm_new();
    hm_insert(c, 122, "abc");
    hm_insert(c, 123, "def");
    hm_insert(c, 0x01000000, "zzz");

    printf("122 = %016lx\n", (uintptr_t)hm_get(c, 122));
    printf("123 = %016lx\n", (uintptr_t)hm_get(c, 123));
    printf("124 = %016lx\n", (uintptr_t)hm_get(c, 124));

    hm_delete(c);
    return 0;
}
