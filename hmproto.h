#include <stdint.h>

#define HM_PROTOS(x) \
    void *x##_new(void);\
    void x##_delete(void *c);\
    \
    int x##_insert(void *c, uint64_t key, void *value);\
    void *x##_remove(void *c, uint64_t key);\
    void *x##_get(void *c, uint64_t key);\
    size_t x##_get_size(void *c);\
    void x##_get_stats(void *c, uint64_t *buf, int nstat);\
    uint64_t x##_debug(void *c, uint64_t arg);

HM_PROTOS(cuckoo)
HM_PROTOS(cuckoo_mutex)
HM_PROTOS(cuckoo_rwlock)
HM_PROTOS(tcradix4)
HM_PROTOS(tcradix5)
HM_PROTOS(tcradix6)
HM_PROTOS(tcradix7)
HM_PROTOS(tcradix8)
HM_PROTOS(tcradix11)
HM_PROTOS(tcradix13)
HM_PROTOS(tcradix16)
HM_PROTOS(tcradix_mutex4)
HM_PROTOS(tcradix_mutex5)
HM_PROTOS(tcradix_mutex6)
HM_PROTOS(tcradix_mutex7)
HM_PROTOS(tcradix_mutex8)
HM_PROTOS(tcradix_mutex11)
HM_PROTOS(tcradix_mutex13)
HM_PROTOS(tcradix_mutex16)
HM_PROTOS(radix4)
HM_PROTOS(radix5)
HM_PROTOS(radix6)
HM_PROTOS(radix7)
HM_PROTOS(radix8)
HM_PROTOS(radix11)
HM_PROTOS(radix13)
HM_PROTOS(radix16)
HM_PROTOS(critbit)
HM_PROTOS(tcradix_valid)
HM_PROTOS(critnib)

void *(*hm_new)(void);
void (*hm_delete)(void *c);
int (*hm_insert)(void *c, uint64_t key, void *value);
void *(*hm_remove)(void *c, uint64_t key);
void *(*hm_get)(void *c, uint64_t key);
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
    HM_SELECT_ONE(x,get_size);\
    HM_SELECT_ONE(x,get_stats);\
    HM_SELECT_ONE(x,debug);\
    hm_name=#x

#define HM_ARR(x,imm) { x##_new, x##_delete, x##_insert, x##_remove, x##_get, \
                        x##_get_size, x##_get_stats, x##_debug, #x, imm }
struct hm
{
    void *(*hm_new)(void);
    void (*hm_delete)(void *c);
    int (*hm_insert)(void *c, uint64_t key, void *value);
    void *(*hm_remove)(void *c, uint64_t key);
    void *(*hm_get)(void *c, uint64_t key);
    size_t (*hm_get_size)(void *c);
    void (*hm_get_stats)(void *c, uint64_t *buf, int nstat);
    uint64_t (*hm_debug)(void *c, uint64_t arg);
    const char *hm_name;
    int hm_immutable;
} hms[21];

void hm_select(int i);
