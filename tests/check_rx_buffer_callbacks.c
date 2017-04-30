#include "check_lightmqtt.h"
#include <stdio.h>

#define ENTRY_COUNT 16

#define PREPARE \
    int res; \
    int bytes_r; \
    lmqtt_rx_buffer_t state; \
    lmqtt_store_t store; \
    lmqtt_message_callbacks_t message_callbacks; \
    void *callbacks_data = 0; \
    int class; \
    lmqtt_store_value_t value; \
    lmqtt_store_entry_t entries[ENTRY_COUNT]; \
    memset(&state, 0, sizeof(state)); \
    memset(&store, 0, sizeof(store)); \
    memset(&message_callbacks, 0, sizeof(message_callbacks)); \
    memset(&value, 0, sizeof(value)); \
    memset(entries, 0, sizeof(entries)); \
    state.store = &store; \
    state.message_callbacks = &message_callbacks; \
    store.get_time = &test_time_get; \
    store.entries = entries; \
    store.capacity = ENTRY_COUNT; \
    value.callback_data = &callbacks_data

static int pingresp_data = 0;
static char topic[100];
static char payload[100];
static test_buffer_t payload_buffer;

static int test_on_connack(void *data, lmqtt_connect_t *connect)
{
    *((void **) data) = connect;
    return 1;
}

static int test_on_suback(void *data, lmqtt_subscribe_t *subscribe)
{
    *((void **) data) = subscribe;
    return 1;
}

static int test_on_publish(void *data, lmqtt_publish_t *publish)
{
    *((void **) data) = publish;
    return 1;
}

static int test_on_pingresp(void *data, void *unused)
{
    *((void **) data) = &pingresp_data;
    return 1;
}

static lmqtt_write_result_t test_write_block(void *data, u8 *buf, int len,
    int *bytes_w)
{
    switch (test_buffer_write(data, buf, len, bytes_w)) {
        case LMQTT_IO_SUCCESS:
            return LMQTT_WRITE_SUCCESS;
        case LMQTT_IO_AGAIN:
            return LMQTT_WRITE_WOULD_BLOCK;
    }
    return LMQTT_WRITE_ERROR;
}

static int test_on_message_received(void *data, lmqtt_publish_t *publish)
{
    char *msg = data;
    sprintf(msg, "qos: %d, retain: %d, topic: %.*s, payload: %.*s",
        publish->qos, publish->retain, publish->topic.len,
        topic, publish->payload.len, publish->payload.buf ? payload :
        (char *) payload_buffer.buf);
    return 1;
}

static lmqtt_allocate_result_t test_on_publish_allocate_topic(void *data,
    lmqtt_publish_t *publish, int len)
{
    publish->topic.len = len;
    publish->topic.buf = topic;
    return LMQTT_ALLOCATE_SUCCESS;
}

static lmqtt_allocate_result_t test_on_publish_allocate_payload(void *data,
    lmqtt_publish_t *publish, int len)
{
    publish->payload.len = len;
    publish->payload.buf = payload;
    return LMQTT_ALLOCATE_SUCCESS;
}

static lmqtt_allocate_result_t test_on_publish_allocate_payload_block(
    void *data, lmqtt_publish_t *publish, int len)
{
    publish->payload.len = len;
    publish->payload.data = &payload_buffer;
    publish->payload.write = &test_write_block;
    return LMQTT_ALLOCATE_SUCCESS;
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
    char msg[100];

    PREPARE;

    memset(msg, 0, sizeof(msg));
    message_callbacks.on_publish = &test_on_message_received;
    message_callbacks.on_publish_data = msg;
    message_callbacks.on_publish_allocate_topic =
        &test_on_publish_allocate_topic;
    message_callbacks.on_publish_allocate_payload =
        &test_on_publish_allocate_payload;

    res = lmqtt_rx_buffer_decode(&state, buf, 8, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);

    ck_assert_str_eq(msg, "qos: 0, retain: 0, topic: X, payload: X");
}
END_TEST

START_TEST(should_decode_qos_and_retain_flag)
{
    u8 *buf = (u8 *) "\x35\x06\x00\x01X\x00\x01X";
    char msg[100];

    PREPARE;

    memset(msg, 0, sizeof(msg));
    message_callbacks.on_publish = &test_on_message_received;
    message_callbacks.on_publish_data = msg;
    message_callbacks.on_publish_allocate_topic =
        &test_on_publish_allocate_topic;
    message_callbacks.on_publish_allocate_payload =
        &test_on_publish_allocate_payload;

    res = lmqtt_rx_buffer_decode(&state, buf, 8, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);

    ck_assert_str_eq(msg, "qos: 2, retain: 1, topic: X, payload: X");
}
END_TEST

START_TEST(should_decode_message_with_blocking_write)
{
    u8 *buf = (u8 *) "\x30\x08\x00\x01T\x03\x04PAY";
    char msg[100];

    PREPARE;

    memset(msg, 0, sizeof(msg));
    message_callbacks.on_publish = &test_on_message_received;
    message_callbacks.on_publish_data = msg;
    message_callbacks.on_publish_allocate_topic =
        &test_on_publish_allocate_topic;
    message_callbacks.on_publish_allocate_payload =
        &test_on_publish_allocate_payload_block;

    payload_buffer.len = 32;
    payload_buffer.available_len = 1;

    res = lmqtt_rx_buffer_decode(&state, &buf[0], 10, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(8, bytes_r);
    ck_assert_ptr_eq(NULL, lmqtt_rx_buffer_get_blocking_str(&state));

    res = lmqtt_rx_buffer_decode(&state, &buf[8], 2, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_AGAIN, res);
    ck_assert_int_eq(0, bytes_r);
    ck_assert_ptr_eq(&state.internal.publish.payload,
        lmqtt_rx_buffer_get_blocking_str(&state));

    payload_buffer.available_len = payload_buffer.len;
    res = lmqtt_rx_buffer_decode(&state, &buf[8], 2, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(2, bytes_r);
    ck_assert_ptr_eq(NULL, lmqtt_rx_buffer_get_blocking_str(&state));

    ck_assert_str_eq("qos: 0, retain: 0, topic: T, payload: PAY", msg);
}
END_TEST

START_TEST(should_decode_pubrel)
{
    u8 *buf = (u8 *) "\x62\x02\x01\x02";

    PREPARE;

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);

    ck_assert_int_eq(1, lmqtt_store_peek(&store, &class, &value));
    ck_assert_int_eq(LMQTT_CLASS_PUBCOMP, class);
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
    ADD_TEST(should_decode_message_with_blocking_write);
    ADD_TEST(should_decode_pubrel);
    ADD_TEST(should_not_call_null_decode_byte);
}
END_TCASE
