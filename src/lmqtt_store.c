#include <lightmqtt/store.h>
#include <string.h>

/******************************************************************************
 * lmqtt_store_t PRIVATE functions
 ******************************************************************************/

static int store_class_uses_packet_id(lmqtt_class_t class)
{
    return class != LMQTT_CLASS_CONNECT && class != LMQTT_CLASS_PING;
}

static int store_find(lmqtt_store_t *store, lmqtt_class_t class, u16 packet_id,
    int *pos)
{
    int i;

    if (!store_class_uses_packet_id(class))
        packet_id = 0;

    for (i = 0; i < store->count; i++) {
        lmqtt_store_entry_t *entry = &store->entries[i];
        if (entry->class == class && entry->packet_id == packet_id) {
            *pos = i;
            return 1;
        }
    }

    *pos = 0;
    return 0;
}

static void store_pop_at(lmqtt_store_t *store, int pos, lmqtt_class_t *class,
    void **data)
{
    lmqtt_store_entry_t *entry = &store->entries[pos];
    *class = entry->class;
    *data = entry->data;
    store->count -= 1;
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

int lmqtt_store_append(lmqtt_store_t *store, lmqtt_class_t class,
    u16 packet_id, void *data)
{

}

int lmqtt_store_save(lmqtt_store_t *store, lmqtt_class_t class, void *data,
    u16 *packet_id)
{
    lmqtt_store_entry_t *entry;

    if (store->count >= LMQTT_STORE_SIZE) {
        *packet_id = 0;
        return 0;
    }

    if (!store_class_uses_packet_id(class)) {
        int i;
        for (i = 0; i < store->count; i++)
            if (store->entries[i].class == class) {
                *packet_id = 0;
                return 0;
            }
    }

    entry = &store->entries[store->count++];
    *packet_id = store->next_packet_id++;

    memset(entry, 0, sizeof(*entry));
    entry->class = class;
    entry->data = data;
    entry->packet_id = *packet_id;

    return 1;
}

int lmqtt_store_peek(lmqtt_store_t *store, lmqtt_class_t class, u16 packet_id,
    void **data)
{
    int pos;

    if (store_find(store, class, packet_id, &pos)) {
        lmqtt_store_entry_t *entry = &store->entries[pos];
        *data = entry->data;
        return 1;
    }

    *data = NULL;
    return 0;
}

int lmqtt_store_pop(lmqtt_store_t *store, lmqtt_class_t class, u16 packet_id,
    void **data)
{
    int pos;

    if (store_find(store, class, packet_id, &pos)) {
        lmqtt_class_t class;
        store_pop_at(store, pos, &class, data);
        return 1;
    }

    *data = NULL;
    return 0;
}

int lmqtt_store_pop_any(lmqtt_store_t *store, lmqtt_class_t *class, void **data)
{
    if (store->count > 0) {
        store_pop_at(store, 0, class, data);
        return 1;
    }

    *class = 0;
    *data = NULL;
    return 0;
}
