#ifndef _LIGHTMQTT_STORE_H_
#define _LIGHTMQTT_STORE_H_

#include <lightmqtt/core.h>
#include <lightmqtt/time.h>

#define LMQTT_STORE_ENTRY_SIZE sizeof(lmqtt_store_entry_t)

typedef int (*lmqtt_store_entry_callback_t)(void *, void *);

typedef struct _lmqtt_store_value_t {
    u16 packet_id;
    void *value;
    lmqtt_store_entry_callback_t callback;
    void *callback_data;
} lmqtt_store_value_t;

typedef struct _lmqtt_store_entry_t {
    int class;
    lmqtt_time_t time;
    lmqtt_store_value_t value;
} lmqtt_store_entry_t;

typedef struct _lmqtt_store_t {
    lmqtt_get_time_t get_time;
    int keep_alive;
    int timeout;
    u16 next_packet_id;
    lmqtt_time_t last_touch;
    int count;
    int pos;
    int capacity;
    lmqtt_store_entry_t *entries;
} lmqtt_store_t;

u16 lmqtt_store_get_id(lmqtt_store_t *store);
int lmqtt_store_count(lmqtt_store_t *store);
int lmqtt_store_has_current(lmqtt_store_t *store);
int lmqtt_store_is_queueable(lmqtt_store_t *store);
int lmqtt_store_append(lmqtt_store_t *store, int class,
    lmqtt_store_value_t *value);
int lmqtt_store_get_at(lmqtt_store_t *store, int pos, int *class,
    lmqtt_store_value_t *value);
int lmqtt_store_delete_at(lmqtt_store_t *store, int pos);
int lmqtt_store_peek(lmqtt_store_t *store, int *class,
    lmqtt_store_value_t *value);
int lmqtt_store_mark_current(lmqtt_store_t *store);
int lmqtt_store_drop_current(lmqtt_store_t *store);
int lmqtt_store_pop_marked_by(lmqtt_store_t *store, int class, u16 packet_id,
    lmqtt_store_value_t *value);
int lmqtt_store_shift(lmqtt_store_t *store, int *class,
    lmqtt_store_value_t *value);
void lmqtt_store_unmark_all(lmqtt_store_t *store);
int lmqtt_store_get_timeout(lmqtt_store_t *store, int *count, long *secs,
    long *nsecs);
void lmqtt_store_touch(lmqtt_store_t *store);

#endif
