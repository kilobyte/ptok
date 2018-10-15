#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "tlog.h"
#include "util.h"

#define TLOG_SIZE (32*1048576)

static struct
{
    uint64_t timestamp;
    uint32_t tid;
    uint32_t act;
    uint64_t data1;
    uint64_t data2;
} tlog_data[TLOG_SIZE];

static uint64_t i;

void tlog_init()
{
    i=0;
}

static inline uint64_t getticks(void)
{
#ifdef __x86_64
     unsigned a, d;
     asm volatile("rdtsc" : "=a" (a), "=d" (d));
     return ((uint64_t)a) | (((uint64_t)d) << 32);
#else
     return 0; // TODO
#endif
}

void tlog(uint32_t tid, uint32_t act, uint64_t data1, uint64_t data2)
{
    uint64_t o = util_fetch_and_add64(&i, 1);
    if (o < TLOG_SIZE)
    {
        tlog_data[o].timestamp=getticks();
        tlog_data[o].tid=tid;
        tlog_data[o].act=act;
        tlog_data[o].data1=data1;
        tlog_data[o].data2=data2;
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
