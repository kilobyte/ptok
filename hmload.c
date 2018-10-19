#include <stdint.h>
#include <stdio.h>
#include "hmproto.h"

struct hm hms[19] =
{
    HM_ARR(cuckoo, 1),
    HM_ARR(cuckoo_mutex, 0),
    HM_ARR(cuckoo_rwlock, 0),
    HM_ARR(tcradix4, 0),
    HM_ARR(tcradix5, 0),
    HM_ARR(tcradix6, 0),
    HM_ARR(tcradix7, 0),
    HM_ARR(tcradix8, 0),
//  HM_ARR(tcradix11, 0),
//  HM_ARR(tcradix13, 0),
//  HM_ARR(tcradix16, 0),
    HM_ARR(tcradix_mutex4, 0),
    HM_ARR(tcradix_mutex5, 0),
    HM_ARR(tcradix_mutex6, 0),
    HM_ARR(tcradix_mutex7, 0),
    HM_ARR(tcradix_mutex8, 0),
//  HM_ARR(tcradix_mutex11, 0),
//  HM_ARR(tcradix_mutex13, 0),
//  HM_ARR(tcradix_mutex16, 0),
    HM_ARR(radix4, 0),
    HM_ARR(radix5, 0),
    HM_ARR(radix6, 0),
    HM_ARR(radix7, 0),
    HM_ARR(radix8, 0),
//  HM_ARR(radix11, 0),
//  HM_ARR(radix13, 0),
//  HM_ARR(radix16, 0),
    HM_ARR(critbit, 0),
};

void hm_select(int i)
{
    hm_new	= hms[i].hm_new;
    hm_delete	= hms[i].hm_delete;
    hm_insert	= hms[i].hm_insert;
    hm_remove	= hms[i].hm_remove;
    hm_get	= hms[i].hm_get;
    hm_get_size	= hms[i].hm_get_size;
    hm_get_stats= hms[i].hm_get_stats;
    hm_debug    = hms[i].hm_debug;
    hm_name	= hms[i].hm_name;
    hm_immutable= hms[i].hm_immutable;
}
