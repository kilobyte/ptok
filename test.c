#include <stdio.h>
#include "hmproto.h"

int main()
{
    HM_SELECT(critnib);
    void *c = hm_new();

    hm_insert(c, 0xdf19b817a2a574f0, "a");
    hm_insert(c, 0xc624b394880c9216, "c");
    hm_remove(c, 0xb2dfb9f1c6c36aa2);

    hm_delete(c);
    return 0;
}
