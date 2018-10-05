#include <stdio.h>
#include "cuckoo.h"

int main()
{
    struct cuckoo *c = cuckoo_new();
    cuckoo_insert(c, 123, "abc");

    printf("123 = %016lx\n", cuckoo_get(c, 123));
    printf("124 = %016lx\n", cuckoo_get(c, 124));

    cuckoo_delete(c);
    return 0;
}
