#ifndef _LIGHTMQTT_STORE_H_
#define _LIGHTMQTT_STORE_H_

#include <lightmqtt/core.h>
#include <lightmqtt/time.h>

#define LMQTT_STORE_SIZE 16

typedef enum {
    LMQTT_CLASS_CONNECT = 200,
    LMQTT_CLASS_PUBLISH,
    LMQTT_CLASS_PUBREL,
    LMQTT_CLASS_SUBSCRIBE,
    LMQTT_CLASS_UNSUBSCRIBE,
    LMQTT_CLASS_PINGREQ,
    LMQTT_CLASS_DISCONNECT
} lmqtt_class_t;

typedef struct _lmqtt_store_entry {
    lmqtt_class_t class;
    u16 packet_id;
    lmqtt_time_t time;
    void *data;
} lmqtt_store_entry_t;

typedef struct _lmqtt_store_t {
    lmqtt_get_time_t get_time;
    int timeout;
    u16 next_packet_id;
    int count;
    int pos;
    lmqtt_store_entry_t entries[LMQTT_STORE_SIZE];
} lmqtt_store_t;

u16 lmqtt_store_get_id(lmqtt_store_t *store);
int lmqtt_store_append(lmqtt_store_t *store, lmqtt_class_t class,
    u16 packet_id, void *data);
int lmqtt_store_peek(lmqtt_store_t *store, lmqtt_class_t *class, void **data);
int lmqtt_store_next(lmqtt_store_t *store);
int lmqtt_store_drop(lmqtt_store_t *store);
int lmqtt_store_pop(lmqtt_store_t *store, lmqtt_class_t class, u16 packet_id,
    void **data);
int lmqtt_store_pop_any(lmqtt_store_t *store, lmqtt_class_t *class,
    void **data);
int lmqtt_get_timeout(lmqtt_store_t *store, long *secs, long *nsecs);

#endif
