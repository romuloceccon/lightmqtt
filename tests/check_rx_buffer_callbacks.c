#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

#define PREPARE \
    int res; \
    int bytes_r; \
    lmqtt_rx_buffer_t state; \
    lmqtt_store_t store; \
    void *callbacks_data = 0; \
    lmqtt_store_value_t value; \
    memset(&state, 0, sizeof(state)); \
    memset(&store, 0, sizeof(store)); \
    memset(&value, 0, sizeof(value)); \
    state.store = &store; \
    state.on_publish = &test_on_message_received; \
    store.get_time = &test_time_get; \
    value.callback_data = &callbacks_data

static int pingresp_data = 0;

int test_on_connack(void *data, lmqtt_connect_t *connect)
{
    *((void **) data) = connect;
    return 1;
}

int test_on_suback(void *data, lmqtt_subscribe_t *subscribe)
{
    *((void **) data) = subscribe;
    return 1;
}

int test_on_publish(void *data, lmqtt_publish_t *publish)
{
    *((void **) data) = publish;
    return 1;
}

int test_on_pingresp(void *data, void *unused)
{
    *((void **) data) = &pingresp_data;
    return 1;
}

int test_on_message_received(void *data, lmqtt_publish_t *publish)
{
    lmqtt_publish_t *dst = (lmqtt_publish_t *) data;
    memcpy(dst, publish, sizeof(*publish));
    return 1;
}

START_TEST(should_call_connack_callback)
{
    lmqtt_connect_t connect;
    u8 *buf = (u8 *) "\x20\x02\x00\x01";

    PREPARE;

    memset(&connect, 0, sizeof(connect));

    value.value = &connect;
    value.callback = (lmqtt_store_entry_callback_t) &test_on_connack;
    lmqtt_store_append(&store, LMQTT_CLASS_CONNECT, &value);
    lmqtt_store_mark_current(&store);

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
    subscribe.count = 1;
    subscribe.subscriptions = subscriptions;

    value.packet_id = 0x0304;
    value.value = &subscribe;
    value.callback = (lmqtt_store_entry_callback_t) &test_on_suback;
    lmqtt_store_append(&store, LMQTT_CLASS_SUBSCRIBE, &value);
    lmqtt_store_mark_current(&store);

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
    subscribe.count = 1;
    subscribe.subscriptions = subscriptions;

    value.packet_id = 0x0304;
    value.value = &subscribe;
    value.callback = (lmqtt_store_entry_callback_t) &test_on_suback;
    lmqtt_store_append(&store, LMQTT_CLASS_UNSUBSCRIBE, &value);
    lmqtt_store_mark_current(&store);

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

    value.packet_id = 0x0506;
    value.value = &publish;
    value.callback = (lmqtt_store_entry_callback_t) &test_on_publish;
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value);
    lmqtt_store_mark_current(&store);

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

    value.packet_id = 0x0a0b;
    value.value = &publish;
    value.callback = (lmqtt_store_entry_callback_t) &test_on_publish;
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_2, &value);
    lmqtt_store_mark_current(&store);

    res = lmqtt_rx_buffer_decode(&state, buf_1, 4, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_ptr_eq(NULL, callbacks_data);

    lmqtt_store_mark_current(&store);
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

    value.packet_id = 0x0a0b;
    value.value = &publish;
    value.callback = (lmqtt_store_entry_callback_t) &test_on_publish;
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_2, &value);
    lmqtt_store_mark_current(&store);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_ptr_eq(NULL, callbacks_data);
}
END_TEST

START_TEST(should_call_pingresp_callback)
{
    u8 *buf = (u8 *) "\xd0\x00";

    PREPARE;

    value.callback = &test_on_pingresp;
    lmqtt_store_append(&store, LMQTT_CLASS_PINGREQ, &value);
    lmqtt_store_mark_current(&store);

    res = lmqtt_rx_buffer_decode(&state, buf, 2, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_ptr_eq(&pingresp_data, callbacks_data);
}
END_TEST

START_TEST(should_call_message_received_callback)
{
    u8 *buf = (u8 *) "\x30\x06\x00\x01X\x02\x03X";
    lmqtt_publish_t publish;

    PREPARE;

    memset(&publish, 0, sizeof(publish));
    state.on_publish_data = &publish;

    res = lmqtt_rx_buffer_decode(&state, buf, 8, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);

    ck_assert_uint_eq(0x203, publish.packet_id);
    ck_assert_uint_eq(0, publish.qos);
    ck_assert_uint_eq(0, publish.retain);
}
END_TEST

START_TEST(should_decode_qos_and_retain_flag)
{
    u8 *buf = (u8 *) "\x35\x06\x00\x01X\x00\x01X";
    lmqtt_publish_t publish;

    PREPARE;

    memset(&publish, 0, sizeof(publish));
    state.on_publish_data = &publish;

    res = lmqtt_rx_buffer_decode(&state, buf, 8, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);

    ck_assert_uint_eq(2, publish.qos);
    ck_assert_uint_eq(1, publish.retain);
}
END_TEST

/* PINGRESP has no decode_byte callback; should return an error */
START_TEST(should_not_call_null_decode_byte)
{
    u8 *buf = (u8 *) "\xd0\x01\x00";

    PREPARE;

    value.callback = &test_on_pingresp;
    lmqtt_store_append(&store, LMQTT_CLASS_PINGREQ, &value);
    lmqtt_store_mark_current(&store);

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
    ADD_TEST(should_call_message_received_callback);
    ADD_TEST(should_decode_qos_and_retain_flag);
    ADD_TEST(should_not_call_null_decode_byte);
}
END_TCASE
