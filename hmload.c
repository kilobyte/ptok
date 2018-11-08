#include <stdint.h>
#include <stdio.h>
#include "hmproto.h"

struct hm hms[3] =
{
    HM_ARR(critbit, 0),
    HM_ARR(critnib, 0),
    HM_ARR(tcradix_atcount, 0),
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
