#include <lightmqtt/time.h>

/******************************************************************************
 * lmqtt_time_t PUBLIC functions
 ******************************************************************************/

int lmqtt_time_get_timeout_to(lmqtt_time_t *tm, lmqtt_get_time_t get_time,
    long when, long *secs, long *nsecs)
{
    long tmo_secs, tmo_nsecs;
    long cur_secs, cur_nsecs;
    long diff;

    if (when == 0) {
        *nsecs = 0;
        *secs = 0;
        return 0;
    }

    tmo_secs = tm->secs + when;
    tmo_nsecs = tm->nsecs;

    get_time(&cur_secs, &cur_nsecs);

    if (tmo_nsecs < cur_nsecs) {
        tmo_nsecs += 1e9;
        tmo_secs -= 1;
    }

    if (cur_secs <= tmo_secs) {
        *nsecs = tmo_nsecs - cur_nsecs;
        *secs = tmo_secs - cur_secs;
    } else {
        *nsecs = 0;
        *secs = 0;
    }
    return 1;
}

void lmqtt_time_touch(lmqtt_time_t *tm, lmqtt_get_time_t get_time)
{
    get_time(&tm->secs, &tm->nsecs);
}
