#include <stdint.h>
#include <stdio.h>
#include "hmproto.h"

struct hm hms[10] =
{
    HM_ARR(cuckoo, 1),
    HM_ARR(cuckoo_mutex, 0),
    HM_ARR(tcradix8, 0),
    HM_ARR(tcradix11, 0),
    HM_ARR(tcradix13, 0),
    HM_ARR(tcradix16, 0),
    HM_ARR(radix8, 0),
    HM_ARR(radix11, 0),
    HM_ARR(radix13, 0),
    HM_ARR(radix16, 0),
};

void hm_select(int i)
{
    hm_new	= hms[i].hm_new;
    hm_delete	= hms[i].hm_delete;
    hm_insert	= hms[i].hm_insert;
    hm_remove	= hms[i].hm_remove;
    hm_get	= hms[i].hm_get;
    hm_get_size	= hms[i].hm_get_size;
    hm_name	= hms[i].hm_name;
    hm_immutable= hms[i].hm_immutable;
}