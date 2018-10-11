#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "util.h"

#define TLOG_SIZE (64*1048576)

static struct
{
    uint32_t tid;
    uint32_t act;
    uint64_t data;
} tlog_data[TLOG_SIZE];

static uint64_t i;

void tlog_init()
{
    i=0;
}

void tlog(uint32_t tid, uint32_t act, uint64_t data)
{
    uint64_t o = util_fetch_and_add64(&i, 1);
    if (o < TLOG_SIZE)
    {
        tlog_data[o].tid=tid;
        tlog_data[o].act=act;
        tlog_data[o].data=data;
    }
}

void tlog_save()
{
    int f = creat("tlog", 0666);
    if (f==-1)
        return (void)fprintf(stderr, "creat(tlog): %m\n");
    uint64_t l = i;
    if (l > TLOG_SIZE)
        l = TLOG_SIZE;
    write(f, tlog_data, l*sizeof(tlog_data[0]));
    close(f);
}

