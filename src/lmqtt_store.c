#include <lightmqtt/store.h>
#include <string.h>

/******************************************************************************
 * lmqtt_store_t PRIVATE functions
 ******************************************************************************/

LMQTT_STATIC int store_find(lmqtt_store_t *store, int class,
    lmqtt_packet_id_t packet_id, size_t *pos)
{
    size_t i;

    for (i = 0; i < store->pos; i++) {
        lmqtt_store_entry_t *entry = &store->entries[i];
        if (entry->class == class && entry->value.packet_id == packet_id) {
            *pos = i;
            return 1;
        }
    }

    *pos = 0;
    return 0;
}

LMQTT_STATIC int store_pop_at(lmqtt_store_t *store, size_t pos, int *class,
    lmqtt_store_value_t *value)
{
    if (!lmqtt_store_get_at(store, pos, class, value))
        return 0;

    store->count -= 1;
    if (store->pos > pos)
        store->pos -= 1;
    memmove(&store->entries[pos], &store->entries[pos + 1],
        sizeof(&store->entries[0]) * (store->capacity - pos - 1));
    return 1;
}

/******************************************************************************
 * lmqtt_store_t PUBLIC functions
 ******************************************************************************/

lmqtt_packet_id_t lmqtt_store_get_id(lmqtt_store_t *store)
{
    return store->next_packet_id++;
}

int lmqtt_store_count(lmqtt_store_t *store)
{
    return store->count;
}

int lmqtt_store_has_current(lmqtt_store_t *store)
{
    int class;
    lmqtt_store_value_t value;

    return lmqtt_store_peek(store, &class, &value);
}

int lmqtt_store_is_queueable(lmqtt_store_t *store)
{
    return store->count < store->capacity;
}

int lmqtt_store_append(lmqtt_store_t *store, int class,
    lmqtt_store_value_t *value)
{
    lmqtt_store_entry_t *entry;

    if (!lmqtt_store_is_queueable(store))
        return 0;

    entry = &store->entries[store->count++];

    entry->class = class;
    if (value)
        memcpy(&entry->value, value, sizeof(entry->value));
    else
        memset(&entry->value, 0, sizeof(entry->value));

    return 1;
}

int lmqtt_store_get_at(lmqtt_store_t *store, size_t pos, int *class,
    lmqtt_store_value_t *value)
{
    lmqtt_store_entry_t *entry;

    if (pos < 0 || pos >= store->count) {
        if (class)
            *class = 0;
        if (value)
            memset(value, 0, sizeof(*value));
        return 0;
    }

    entry = &store->entries[pos];
    if (class)
        *class = entry->class;
    if (value)
        memcpy(value, &entry->value, sizeof(entry->value));
    return 1;
}

int lmqtt_store_delete_at(lmqtt_store_t *store, size_t pos)
{
    return store_pop_at(store, pos, NULL, NULL);
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
    return store_pop_at(store, store->pos, NULL, NULL);
}

int lmqtt_store_peek(lmqtt_store_t *store, int *class,
    lmqtt_store_value_t *value)
{
    return lmqtt_store_get_at(store, store->pos, class, value);
}

int lmqtt_store_pop_marked_by(lmqtt_store_t *store, int class,
    lmqtt_packet_id_t packet_id, lmqtt_store_value_t *value)
{
    size_t pos;

    if (store_find(store, class, packet_id, &pos)) {
        return store_pop_at(store, pos, NULL, value);
    }

    if (value)
        memset(value, 0, sizeof(*value));
    return 0;
}

int lmqtt_store_shift(lmqtt_store_t *store, int *class,
    lmqtt_store_value_t *value)
{
    return store_pop_at(store, 0, class, value);
}

void lmqtt_store_unmark_all(lmqtt_store_t *store)
{
    store->pos = 0;
}

int lmqtt_store_get_timeout(lmqtt_store_t *store, size_t *count, long *secs,
    long *nsecs)
{
    lmqtt_time_t *tm = &store->last_touch;
    unsigned short when = store->count > 0 ? store->timeout : store->keep_alive;

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
