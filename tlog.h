#include <stdint.h>

void tlog_init();
void tlog(uint32_t tid, uint32_t act, uint64_t data1, uint64_t data2);
void tlog_save();
