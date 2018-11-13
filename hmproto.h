#include <stdint.h>

#define HM_PROTOS(x) \
    void *x##_new(void);\
    void x##_delete(void *c);\
    \
    int x##_insert(void *c, uint64_t key, void *value);\
    void *x##_remove(void *c, uint64_t key);\
    void *x##_get(void *c, uint64_t key);\
    void *x##_find_le(void *c, uint64_t key);\
    size_t x##_get_size(void *c);\
    void x##_get_stats(void *c, uint64_t *buf, int nstat);\
    uint64_t x##_debug(void *c, uint64_t arg);

HM_PROTOS(critbit)
HM_PROTOS(critnib)
HM_PROTOS(tcradix)
HM_PROTOS(critnib_atcount)

void *(*hm_new)(void);
void (*hm_delete)(void *c);
int (*hm_insert)(void *c, uint64_t key, void *value);
void *(*hm_remove)(void *c, uint64_t key);
void *(*hm_get)(void *c, uint64_t key);
void *(*hm_find_le)(void *c, uint64_t key);
size_t (*hm_get_size)(void *c);
void (*hm_get_stats)(void *c, uint64_t *buf, int nstat);
uint64_t (*hm_debug)(void *c, uint64_t arg);
const char *hm_name;
int hm_immutable;

#define HM_SELECT_ONE(x,f) hm_##f=x##_##f
#define HM_SELECT(x) \
    HM_SELECT_ONE(x,new);\
    HM_SELECT_ONE(x,delete);\
    HM_SELECT_ONE(x,insert);\
    HM_SELECT_ONE(x,remove);\
    HM_SELECT_ONE(x,get);\
    HM_SELECT_ONE(x,find_le);\
    HM_SELECT_ONE(x,get_size);\
    HM_SELECT_ONE(x,get_stats);\
    HM_SELECT_ONE(x,debug);\
    hm_name=#x

#define HM_ARR(x,imm) { x##_new, x##_delete, x##_insert, x##_remove, x##_get, \
                        x##_find_le, x##_get_size, x##_get_stats, x##_debug, #x, imm }
struct hm
{
    void *(*hm_new)(void);
    void (*hm_delete)(void *c);
    int (*hm_insert)(void *c, uint64_t key, void *value);
    void *(*hm_remove)(void *c, uint64_t key);
    void *(*hm_get)(void *c, uint64_t key);
    void *(*hm_find_le)(void *c, uint64_t key);
    size_t (*hm_get_size)(void *c);
    void (*hm_get_stats)(void *c, uint64_t *buf, int nstat);
    uint64_t (*hm_debug)(void *c, uint64_t arg);
    const char *hm_name;
    int hm_immutable;
} hms[4];

void hm_select(int i);
