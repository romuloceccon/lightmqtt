#include "check_lightmqtt.h"

#define PREPARE \
    int data[10]; \
    lmqtt_rx_buffer_t state; \
    lmqtt_store_t store; \
    lmqtt_store_value_t value; \
    test_cb_results_t cb_results; \
    memset(&data, 0, sizeof(data)); \
    memset(&state, 0, sizeof(state)); \
    memset(&store, 0, sizeof(store)); \
    memset(&value, 0, sizeof(value)); \
    memset(&cb_results, 0xcc, sizeof(cb_results)); \
    state.store = &store; \
    store.get_time = &test_time_get; \
    value.callback_data = &cb_results; \
    cb_results.count = 0

typedef struct _test_cb_results_t {
    int count;
    void *results[10];
} test_cb_results_t;

int test_set_result(void *data, void *packet_data)
{
    test_cb_results_t *results = (test_cb_results_t *) data;
    void **result = &results->results[results->count++];

    *result = packet_data;

    return 1;
}

int test_on_connack(void *data, lmqtt_connect_t *connect)
{
    return test_set_result(data, connect);
}

int test_on_suback(void *data, lmqtt_subscribe_t *subscribe)
{
    return test_set_result(data, subscribe);
}

START_TEST(should_finish_unused_buffer)
{
    PREPARE;

    /* should not segfault */
    lmqtt_rx_buffer_finish(&state);
    ck_assert_int_eq(0, cb_results.count);
}
END_TEST

START_TEST(should_finish_unused_store_items)
{
    PREPARE;

    // sent item with callback
    value.value = &data[0];
    value.callback = (lmqtt_store_entry_callback_t) &test_on_connack;
    lmqtt_store_append(&store, LMQTT_CLASS_CONNECT, 0, &value);
    lmqtt_store_mark_current(&store);
    // unsent item with callback
    value.value = &data[1];
    value.callback = (lmqtt_store_entry_callback_t) &test_on_suback;
    lmqtt_store_append(&store, LMQTT_CLASS_SUBSCRIBE, 1, &value);
    // unsent item without callback
    value.value = &data[2];
    value.callback = NULL;
    lmqtt_store_append(&store, LMQTT_CLASS_PINGREQ, 2, &value);

    lmqtt_rx_buffer_finish(&state);
    // should call callbacks only for items with callback
    ck_assert_int_eq(2, cb_results.count);
    ck_assert_ptr_eq(&data[0], cb_results.results[0]);
    ck_assert_ptr_eq(&data[1], cb_results.results[1]);
}
END_TEST

START_TCASE("Rx buffer finish")
{
    ADD_TEST(should_finish_unused_buffer);
    ADD_TEST(should_finish_unused_store_items);
}
END_TCASE
