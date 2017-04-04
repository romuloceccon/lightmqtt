#include "check_lightmqtt.h"

#define PREPARE \
    int data[10]; \
    lmqtt_rx_buffer_t state; \
    lmqtt_store_t store; \
    lmqtt_rx_buffer_callbacks_t callbacks; \
    test_cb_results_t cb_results; \
    memset(&data, 0, sizeof(data)); \
    memset(&state, 0, sizeof(state)); \
    memset(&store, 0, sizeof(store)); \
    memset(&callbacks, 0, sizeof(callbacks)); \
    memset(&cb_results, 0xcc, sizeof(cb_results)); \
    state.store = &store; \
    state.callbacks_data = &cb_results; \
    state.callbacks = &callbacks; \
    callbacks.on_connack = &test_on_connack; \
    callbacks.on_suback = &test_on_suback; \
    callbacks.on_pingresp = &test_on_pingresp; \
    store.get_time = &test_time_get; \
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

int test_on_pingresp(void *data)
{
    return test_set_result(data, 0);
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

    // sent item with data
    lmqtt_store_append(&store, LMQTT_CLASS_CONNECT, 0, &data[0]);
    lmqtt_store_mark_current(&store);
    // unsent item with data
    lmqtt_store_append(&store, LMQTT_CLASS_SUBSCRIBE, 1, &data[1]);
    // unsent item without data
    lmqtt_store_append(&store, LMQTT_CLASS_PINGREQ, 2, 0);

    lmqtt_rx_buffer_finish(&state);
    // should call callbacks only for items with data
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
