#ifndef _LIGHTMQTT_TIME_H_
#define _LIGHTMQTT_TIME_H_

#include <lightmqtt/core.h>

typedef lmqtt_io_result_t (*lmqtt_get_time_t)(long *, long *);

typedef struct _lmqtt_time_t {
    long secs;
    long nsecs;
} lmqtt_time_t;

int lmqtt_time_get_timeout_to(lmqtt_time_t *tm, lmqtt_get_time_t get_time,
    unsigned short when, long *secs, long *nsecs);

void lmqtt_time_touch(lmqtt_time_t *tm, lmqtt_get_time_t get_time);

#endif
