#include <stdint.h>

#define HM_PROTOS(x) \
    void *x##_new(void);\
    void x##_delete(void *c);\
    \
    int x##_insert(void *c, uint64_t key, void *value);\
    void *x##_remove(void *c, uint64_t key);\
    void *x##_get(void *c, uint64_t key);\
    size_t x##_get_size(void *c);\

HM_PROTOS(cuckoo)

void *(*hm_new)(void);
void (*hm_delete)(void *c);
int (*hm_insert)(void *c, uint64_t key, void *value);
void *(*hm_remove)(void *c, uint64_t key);
void *(*hm_get)(void *c, uint64_t key);
size_t (*hm_get_size)(void *c);

#define HM_SELECT_ONE(x,f) hm_##f=x##_##f
#define HM_SELECT(x) \
    HM_SELECT_ONE(x,new);\
    HM_SELECT_ONE(x,delete);\
    HM_SELECT_ONE(x,insert);\
    HM_SELECT_ONE(x,remove);\
    HM_SELECT_ONE(x,get);\
    HM_SELECT_ONE(x,get_size);\

