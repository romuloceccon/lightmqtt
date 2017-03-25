#include <lightmqtt/store.h>
#include <string.h>

/******************************************************************************
 * lmqtt_store_t PRIVATE functions
 ******************************************************************************/

static int store_find(lmqtt_store_t *store, int class, u16 packet_id, int *pos)
{
    int i;

    for (i = 0; i < store->pos; i++) {
        lmqtt_store_entry_t *entry = &store->entries[i];
        if (entry->class == class && entry->packet_id == packet_id) {
            *pos = i;
            return 1;
        }
    }

    *pos = 0;
    return 0;
}

static void store_pop_at(lmqtt_store_t *store, int pos, int *class, void **data)
{
    lmqtt_store_entry_t *entry = &store->entries[pos];
    *class = entry->class;
    *data = entry->data;
    store->count -= 1;
    if (store->pos > pos)
        store->pos -= 1;
    memmove(&store->entries[pos], &store->entries[pos + 1],
        sizeof(*entry) * (LMQTT_STORE_SIZE - pos - 1));
}

/******************************************************************************
 * lmqtt_store_t PUBLIC functions
 ******************************************************************************/

u16 lmqtt_store_get_id(lmqtt_store_t *store)
{
    return store->next_packet_id++;
}

int lmqtt_store_append(lmqtt_store_t *store, int class, u16 packet_id,
    void *data)
{
    lmqtt_store_entry_t *entry;

    if (store->count >= LMQTT_STORE_SIZE)
        return 0;

    entry = &store->entries[store->count++];

    memset(entry, 0, sizeof(*entry));
    entry->class = class;
    entry->data = data;
    entry->packet_id = packet_id;

    return 1;
}

int lmqtt_store_next(lmqtt_store_t *store)
{
    if (store->pos < store->count) {
        lmqtt_store_entry_t *entry = &store->entries[store->pos++];
        lmqtt_time_touch(&entry->time, store->get_time);
        return 1;
    }

    return 0;
}

int lmqtt_store_drop(lmqtt_store_t *store)
{
    int class;
    void *data;

    if (store->pos < store->count) {
        store_pop_at(store, store->pos, &class, &data);
        return 1;
    }

    return 0;
}

int lmqtt_store_peek(lmqtt_store_t *store, int *class, void **data)
{
    if (store->pos < store->count) {
        lmqtt_store_entry_t *entry = &store->entries[store->pos];
        *class = entry->class;
        *data = entry->data;
        return 1;
    }

    *class = 0;
    *data = NULL;
    return 0;
}

int lmqtt_store_pop(lmqtt_store_t *store, int class, u16 packet_id, void **data)
{
    int pos;

    if (store_find(store, class, packet_id, &pos)) {
        int class;
        store_pop_at(store, pos, &class, data);
        return 1;
    }

    *data = NULL;
    return 0;
}

int lmqtt_store_pop_any(lmqtt_store_t *store, int *class, void **data)
{
    if (store->count > 0) {
        store_pop_at(store, 0, class, data);
        return 1;
    }

    *class = 0;
    *data = NULL;
    return 0;
}

int lmqtt_store_get_timeout(lmqtt_store_t *store, int *count, long *secs,
    long *nsecs)
{
    lmqtt_time_t *tm;
    int when = 0;

    if (store->pos > 0) {
        tm = &store->entries[0].time;
        when = store->timeout;
    } else {
        tm = &store->last_touch;
        when = store->keep_alive;
    }

    if (when == 0) {
        *count = 0;
        *secs = 0;
        *nsecs = 0;
        return 0;
    }

    *count = store->pos;
    return lmqtt_time_get_timeout_to(tm, store->get_time, when, secs, nsecs);
}

void lmqtt_store_touch(lmqtt_store_t *store)
{
    lmqtt_time_touch(&store->last_touch, store->get_time);
}
