#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

#define PREPARE \
    int res; \
    int bytes_r; \
    lmqtt_rx_buffer_t state; \
    lmqtt_store_t store; \
    lmqtt_rx_buffer_callbacks_t callbacks; \
    void *callbacks_data = 0; \
    memset(&state, 0, sizeof(state)); \
    memset(&store, 0, sizeof(store)); \
    memset(&callbacks, 0, sizeof(callbacks)); \
    state.store = &store; \
    state.callbacks_data = &callbacks_data; \
    state.callbacks = &callbacks; \
    store.get_time = &test_time_get

static int pingresp_data = 0;

int test_on_connack(void *data, lmqtt_connect_t *connect)
{
    *((void **) data) = connect;
}

int test_on_suback(void *data, lmqtt_subscribe_t *subscribe)
{
    *((void **) data) = subscribe;
}

int test_on_publish(void *data, lmqtt_publish_t *publish)
{
    *((void **) data) = publish;
}

int test_on_pingresp(void *data)
{
    *((void **) data) = &pingresp_data;
}

START_TEST(should_call_connack_callback)
{
    lmqtt_connect_t connect;
    u8 *buf = (u8 *) "\x20\x02\x00\x01";

    PREPARE;

    memset(&connect, 0, sizeof(connect));
    callbacks.on_connack = &test_on_connack;

    lmqtt_store_append(&store, LMQTT_CLASS_CONNECT, 0, &connect);
    lmqtt_store_next(&store);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_ptr_eq(&connect, callbacks_data);
    ck_assert_int_eq(1, connect.response.return_code);
}
END_TEST

START_TEST(should_call_suback_callback)
{
    lmqtt_subscribe_t subscribe;
    lmqtt_subscription_t subscriptions[1];
    u8 *buf = (u8 *) "\x90\x03\x03\x04\x02";

    PREPARE;

    memset(&subscribe, 0, sizeof(subscribe));
    callbacks.on_suback = &test_on_suback;
    subscribe.count = 1;
    subscribe.subscriptions = subscriptions;

    lmqtt_store_append(&store, LMQTT_CLASS_SUBSCRIBE, 0x0304, &subscribe);
    lmqtt_store_next(&store);

    res = lmqtt_rx_buffer_decode(&state, buf, 5, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_ptr_eq(&subscribe, callbacks_data);
    ck_assert_int_eq(2, subscriptions[0].return_code);
}
END_TEST

START_TEST(should_call_unsuback_callback)
{
    lmqtt_subscribe_t subscribe;
    lmqtt_subscription_t subscriptions[1];
    u8 *buf = (u8 *) "\xb0\x02\x03\x04";

    PREPARE;

    memset(&subscribe, 0, sizeof(subscribe));
    callbacks.on_unsuback = &test_on_suback;
    subscribe.count = 1;
    subscribe.subscriptions = subscriptions;

    lmqtt_store_append(&store, LMQTT_CLASS_UNSUBSCRIBE, 0x0304, &subscribe);
    lmqtt_store_next(&store);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_ptr_eq(&subscribe, callbacks_data);
}
END_TEST

START_TEST(should_call_publish_callback_with_qos_1)
{
    lmqtt_publish_t publish;
    u8 *buf = (u8 *) "\x40\x02\x05\x06";

    PREPARE;

    memset(&publish, 0, sizeof(publish));
    publish.packet_id = 0x0506;
    publish.qos = 1;
    publish.topic.buf = "a";
    publish.topic.len = 1;
    callbacks.on_publish_tx = &test_on_publish;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, 0x0506, &publish);
    lmqtt_store_next(&store);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_ptr_eq(&publish, callbacks_data);
}
END_TEST

START_TEST(should_call_publish_callback_with_qos_2)
{
    lmqtt_publish_t publish;
    u8 *buf_1 = (u8 *) "\x50\x02\x0a\x0b";
    u8 *buf_2 = (u8 *) "\x70\x02\x0a\x0b";

    PREPARE;

    memset(&publish, 0, sizeof(publish));
    publish.packet_id = 0x0a0b;
    publish.qos = 2;
    publish.topic.buf = "a";
    publish.topic.len = 1;
    callbacks.on_publish_tx = &test_on_publish;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_2, 0x0a0b, &publish);
    lmqtt_store_next(&store);

    res = lmqtt_rx_buffer_decode(&state, buf_1, 4, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_ptr_eq(NULL, callbacks_data);

    lmqtt_store_next(&store);
    res = lmqtt_rx_buffer_decode(&state, buf_2, 4, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_ptr_eq(&publish, callbacks_data);
}
END_TEST

START_TEST(should_not_release_publish_with_qos_2_without_pubrec)
{
    lmqtt_publish_t publish;
    u8 *buf = (u8 *) "\x70\x02\x0a\x0b";

    PREPARE;

    memset(&publish, 0, sizeof(publish));
    publish.packet_id = 0x0a0b;
    publish.qos = 2;
    publish.topic.buf = "a";
    publish.topic.len = 1;
    callbacks.on_publish_tx = &test_on_publish;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_2, 0x0a0b, &publish);
    lmqtt_store_next(&store);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_ptr_eq(NULL, callbacks_data);
}
END_TEST

START_TEST(should_call_pingresp_callback)
{
    u8 *buf = (u8 *) "\xd0\x00";

    PREPARE;

    callbacks.on_pingresp = &test_on_pingresp;

    lmqtt_store_append(&store, LMQTT_CLASS_PINGREQ, 0, NULL);
    lmqtt_store_next(&store);

    res = lmqtt_rx_buffer_decode(&state, buf, 2, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_ptr_eq(&pingresp_data, callbacks_data);
}
END_TEST

/* PINGRESP has no decode_byte callback; should return an error */
START_TEST(should_not_call_null_decode_byte)
{
    u8 *buf = (u8 *) "\xd0\x01\x00";

    PREPARE;

    callbacks.on_pingresp = &test_on_pingresp;

    lmqtt_store_append(&store, LMQTT_CLASS_PINGREQ, 0, NULL);
    lmqtt_store_next(&store);

    res = lmqtt_rx_buffer_decode(&state, buf, 3, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_ptr_eq(0, callbacks_data);
}
END_TEST

START_TCASE("Rx buffer callbacks")
{
    ADD_TEST(should_call_connack_callback);
    ADD_TEST(should_call_suback_callback);
    ADD_TEST(should_call_unsuback_callback);
    ADD_TEST(should_call_publish_callback_with_qos_1);
    ADD_TEST(should_call_publish_callback_with_qos_2);
    ADD_TEST(should_not_release_publish_with_qos_2_without_pubrec);
    ADD_TEST(should_call_pingresp_callback);
    ADD_TEST(should_not_call_null_decode_byte);
}
END_TCASE
