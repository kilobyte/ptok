#include <stdio.h>
#include "hmproto.h"

int main()
{
    HM_SELECT(cuckoo);
    void *c = hm_new();
    hm_insert(c, 123, "abc");

    printf("123 = %016lx\n", (uintptr_t)hm_get(c, 123));
    printf("124 = %016lx\n", (uintptr_t)hm_get(c, 124));

    hm_delete(c);
    return 0;
}
