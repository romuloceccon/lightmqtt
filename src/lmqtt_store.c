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

static int store_pop_at(lmqtt_store_t *store, int pos, int *class, void **data)
{
    if (!lmqtt_store_get_at(store, pos, class, data))
        return 0;

    store->count -= 1;
    if (store->pos > pos)
        store->pos -= 1;
    memmove(&store->entries[pos], &store->entries[pos + 1],
        sizeof(&store->entries[0]) * (LMQTT_STORE_SIZE - pos - 1));
    return 1;
}

/******************************************************************************
 * lmqtt_store_t PUBLIC functions
 ******************************************************************************/

u16 lmqtt_store_get_id(lmqtt_store_t *store)
{
    return store->next_packet_id++;
}

int lmqtt_store_count(lmqtt_store_t *store)
{
    return store->count;
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
    lmqtt_time_touch(&entry->time, store->get_time);

    return 1;
}

int lmqtt_store_get_at(lmqtt_store_t *store, int pos, int *class, void **data)
{
    lmqtt_store_entry_t *entry;

    if (pos < 0 || pos >= store->count) {
        *class = 0;
        *data = NULL;
        return 0;
    }

    entry = &store->entries[pos];
    *class = entry->class;
    *data = entry->data;
    return 1;
}

int lmqtt_store_delete_at(lmqtt_store_t *store, int pos)
{
    int class;
    void *data;

    return store_pop_at(store, pos, &class, &data);
}

int lmqtt_store_mark_current(lmqtt_store_t *store)
{
    if (store->pos < store->count) {
        store->pos++;
        return 1;
    }

    return 0;
}

int lmqtt_store_drop_current(lmqtt_store_t *store)
{
    int class;
    void *data;

    return store_pop_at(store, store->pos, &class, &data);
}

int lmqtt_store_peek(lmqtt_store_t *store, int *class, void **data)
{
    return lmqtt_store_get_at(store, store->pos, class, data);
}

int lmqtt_store_pop_marked_by(lmqtt_store_t *store, int class, u16 packet_id,
    void **data)
{
    int pos;

    if (store_find(store, class, packet_id, &pos)) {
        int class_tmp;
        return store_pop_at(store, pos, &class_tmp, data);
    }

    *data = NULL;
    return 0;
}

int lmqtt_store_shift(lmqtt_store_t *store, int *class, void **data)
{
    return store_pop_at(store, 0, class, data);
}

void lmqtt_store_unmark_all(lmqtt_store_t *store)
{
    store->pos = 0;
}

int lmqtt_store_get_timeout(lmqtt_store_t *store, int *count, long *secs,
    long *nsecs)
{
    lmqtt_time_t *tm = NULL;
    int when = 0;

    if (store->count > 0) {
        tm = &store->entries[0].time;
        when = store->timeout;
    } else {
        tm = &store->last_touch;
        when = store->keep_alive;
    }

    if (when == 0 || tm->secs == 0 && tm->nsecs == 0) {
        *count = 0;
        *secs = 0;
        *nsecs = 0;
        return 0;
    }

    *count = store->count;
    return lmqtt_time_get_timeout_to(tm, store->get_time, when, secs, nsecs);
}

void lmqtt_store_touch(lmqtt_store_t *store)
{
    lmqtt_time_touch(&store->last_touch, store->get_time);
}
