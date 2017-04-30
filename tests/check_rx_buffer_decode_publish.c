#include "check_lightmqtt.h"
#include <stdio.h>

#define ENTRY_COUNT 16

static lmqtt_rx_buffer_t state;
static lmqtt_store_t store;
static lmqtt_message_callbacks_t message_callbacks;
static int class;
static lmqtt_store_value_t value;
static lmqtt_store_entry_t entries[ENTRY_COUNT];
static lmqtt_publish_t *publish;
static char topic[1000];
static lmqtt_allocate_result_t allocate_topic_result;
static char payload[1000];
static lmqtt_allocate_result_t allocate_payload_result;
static void *publish_deallocated;
static int allocate_topic_count;
static int allocate_payload_count;
static int deallocate_count;
static char message_received[1000];

static int test_on_publish(void *data, lmqtt_publish_t *publish)
{
    lmqtt_publish_t **dst = (lmqtt_publish_t **) data;
    *dst = publish;
    sprintf(message_received, "topic: %.*s, payload: %.*s", publish->topic.len,
        publish->topic.buf, publish->payload.len, publish->payload.buf);
    return 1;
}

static lmqtt_allocate_result_t test_on_publish_allocate_topic(void *data,
    lmqtt_publish_t *publish, int len)
{
    if (allocate_topic_result == LMQTT_ALLOCATE_SUCCESS) {
        publish->topic.len = len;
        publish->topic.buf = topic;
    }
    allocate_topic_count++;
    return allocate_topic_result;
}

static lmqtt_allocate_result_t test_on_publish_allocate_payload(void *data,
    lmqtt_publish_t *publish, int len)
{
    if (allocate_payload_result == LMQTT_ALLOCATE_SUCCESS) {
        publish->payload.len = len;
        publish->payload.buf = payload;
    }
    allocate_payload_count++;
    return allocate_payload_result;
}

static void test_on_publish_deallocate(void *data, lmqtt_publish_t *publish)
{
    /* Make sure nobody tries to use strings after deallocate */
    memset(&publish->topic, 0xcc, sizeof(publish->topic));
    memset(&publish->payload, 0xcc, sizeof(publish->payload));
    publish_deallocated = data;
    deallocate_count++;
}

static lmqtt_write_result_t test_write_fail(void *data, u8 *buf, int len,
    int *bytes_w)
{
    lmqtt_string_t *str = data;
    return str->internal.pos >= 1 ? LMQTT_WRITE_ERROR : LMQTT_WRITE_SUCCESS;
}

static lmqtt_allocate_result_t test_on_publish_allocate_topic_fail(void *data,
    lmqtt_publish_t *publish, int len)
{
    publish->topic.len = len;
    publish->topic.data = &publish->topic;
    publish->topic.write = &test_write_fail;
    allocate_topic_count++;
    return LMQTT_ALLOCATE_SUCCESS;
}

static void init_state()
{
    memset(&state, 0, sizeof(state));
    memset(&store, 0, sizeof(store));
    memset(&message_callbacks, 0, sizeof(message_callbacks));
    memset(&topic, 0, sizeof(topic));
    memset(&payload, 0, sizeof(payload));
    memset(entries, 0, sizeof(entries));
    memset(message_received, 0, sizeof(message_received));
    state.store = &store;
    state.message_callbacks = &message_callbacks;
    message_callbacks.on_publish = &test_on_publish;
    message_callbacks.on_publish_allocate_topic =
        &test_on_publish_allocate_topic;
    message_callbacks.on_publish_allocate_payload =
        &test_on_publish_allocate_payload;
    message_callbacks.on_publish_deallocate = &test_on_publish_deallocate;
    message_callbacks.on_publish_data = &publish;
    store.get_time = &test_time_get;
    store.entries = entries;
    store.capacity = ENTRY_COUNT;
    allocate_topic_result = LMQTT_ALLOCATE_SUCCESS;
    allocate_payload_result = LMQTT_ALLOCATE_SUCCESS;
    allocate_topic_count = 0;
    allocate_payload_count = 0;
    deallocate_count = 0;
    publish_deallocated = NULL;
    publish = NULL;
}

static void do_decode(u8 val, lmqtt_decode_result_t exp)
{
    int res = rx_buffer_decode_publish(&state, val);
    if (res == LMQTT_DECODE_CONTINUE || res == LMQTT_DECODE_FINISHED)
        state.internal.remain_buf_pos++;
    ck_assert_int_eq(exp, res);
}

static void do_decode_buffer(char *buf, int len)
{
    int i;

    state.internal.remain_buf_pos = 0;
    state.internal.header.remaining_length = len;
    state.internal.packet_id = 0;
    state.internal.ignore_publish = 0;
    memset(&state.internal.publish, 0, sizeof(state.internal.publish));

    for (i = 0; i < len - 1; i++)
        do_decode((u8) buf[i], LMQTT_DECODE_CONTINUE);

    do_decode((u8) buf[len - 1], LMQTT_DECODE_FINISHED);
}

START_TEST(should_decode_one_byte_topic_and_payload)
{
    init_state();

    do_decode_buffer("\x00\x01x\x02\xffx", 6);

    ck_assert_uint_eq(1, state.internal.topic_len);
    ck_assert_uint_eq(0x2ff, state.internal.packet_id);
}
END_TEST

START_TEST(should_decode_long_topic_and_payload)
{
    int i;

    init_state();

    state.internal.header.remaining_length = 0x203 + 6;

    do_decode(2, LMQTT_DECODE_CONTINUE);
    do_decode(3, LMQTT_DECODE_CONTINUE);
    for (i = 0; i < 0x203; i++)
        do_decode((u8) 'x', LMQTT_DECODE_CONTINUE);
    do_decode(3, LMQTT_DECODE_CONTINUE);
    do_decode(254, LMQTT_DECODE_CONTINUE);
    do_decode((u8) 'a', LMQTT_DECODE_CONTINUE);
    do_decode((u8) 'b', LMQTT_DECODE_FINISHED);

    ck_assert_uint_eq(0x203, state.internal.topic_len);
    ck_assert_uint_eq(0x3fe, state.internal.packet_id);
}
END_TEST

START_TEST(should_decode_empty_topic)
{
    init_state();

    state.internal.header.remaining_length = 10;

    do_decode(0, LMQTT_DECODE_CONTINUE);
    do_decode(0, LMQTT_DECODE_ERROR);
}
END_TEST

START_TEST(should_decode_invalid_remaining_length)
{
    init_state();

    state.internal.header.remaining_length = 8;

    do_decode(0, LMQTT_DECODE_CONTINUE);
    do_decode(5, LMQTT_DECODE_ERROR);
}
END_TEST

START_TEST(should_decode_empty_payload)
{
    init_state();

    do_decode_buffer("\x00\x03xxx\x00\x01", 7);
}
END_TEST

START_TEST(should_not_reply_to_publish_with_qos_0)
{
    init_state();

    state.internal.header.qos = 0;
    do_decode_buffer("\x00\x01X\x00\x00X", 6);

    ck_assert_int_eq(0, lmqtt_store_peek(&store, &class, &value));
}
END_TEST

START_TEST(should_reply_to_publish_with_qos_1)
{
    init_state();

    state.internal.header.qos = 1;
    do_decode_buffer("\x00\x01X\x02\x05X", 6);

    ck_assert_int_eq(1, lmqtt_store_peek(&store, &class, &value));
    ck_assert_int_eq(LMQTT_CLASS_PUBACK, class);
    ck_assert_int_eq(0x0205, value.packet_id);
}
END_TEST

START_TEST(should_reply_to_publish_with_qos_2)
{
    init_state();

    state.internal.header.qos = 2;
    do_decode_buffer("\x00\x01X\x02\x05X", 6);

    ck_assert_int_eq(1, lmqtt_store_peek(&store, &class, &value));
    ck_assert_int_eq(LMQTT_CLASS_PUBREC, class);
    ck_assert_int_eq(0x0205, value.packet_id);
}
END_TEST

START_TEST(should_call_callback_multiple_times_with_qos_1)
{
    init_state();

    state.internal.header.qos = 1;
    do_decode_buffer("\x00\x01X\x02\x06X", 6);

    ck_assert_ptr_eq(publish, &state.internal.publish);
    ck_assert_str_eq("topic: X, payload: X", message_received);

    publish = NULL;
    memset(message_received, 0, sizeof(message_received));
    do_decode_buffer("\x00\x01X\x02\x06X", 6);

    ck_assert_ptr_eq(&state.internal.publish, publish);
    ck_assert_str_eq("topic: X, payload: X", message_received);
}
END_TEST

START_TEST(should_not_call_callback_multiple_times_with_qos_2)
{
    init_state();

    state.internal.header.qos = 2;
    do_decode_buffer("\x00\x01X\x02\x06X", 6);

    ck_assert_ptr_eq(publish, &state.internal.publish);

    publish = NULL;
    do_decode_buffer("\x00\x01X\x02\x06X", 6);

    ck_assert_ptr_eq(NULL, publish);
}
END_TEST

START_TEST(should_call_allocate_callbacks)
{
    init_state();

    state.internal.header.qos = 1;
    do_decode_buffer("\x00\x03TOP\x02\x06PAY", 10);

    ck_assert_str_eq("TOP", topic);
    ck_assert_str_eq("PAY", payload);
    ck_assert_ptr_eq(publish, &state.internal.publish);
    ck_assert_str_eq("topic: TOP, payload: PAY", message_received);

    ck_assert_int_eq(1, allocate_topic_count);
    ck_assert_int_eq(1, allocate_payload_count);
    ck_assert_int_eq(1, deallocate_count);
}
END_TEST

START_TEST(should_ignore_message_if_topic_is_ignored)
{
    init_state();

    allocate_topic_result = LMQTT_ALLOCATE_IGNORE;
    state.internal.header.qos = 1;
    do_decode_buffer("\x00\x03TOP\x02\x06PAY", 10);

    ck_assert(!publish);
    ck_assert_str_eq("", message_received);

    ck_assert_int_eq(1, allocate_topic_count);
    ck_assert_int_eq(0, allocate_payload_count);
    ck_assert_int_eq(0, deallocate_count);
}
END_TEST

START_TEST(should_ignore_message_if_payload_is_ignored)
{
    init_state();

    allocate_payload_result = LMQTT_ALLOCATE_IGNORE;
    state.internal.header.qos = 1;
    do_decode_buffer("\x00\x03TOP\x02\x06PAY", 10);

    ck_assert(!publish);
    ck_assert_str_eq("", message_received);

    ck_assert_int_eq(1, allocate_topic_count);
    ck_assert_int_eq(1, allocate_payload_count);
    ck_assert_int_eq(0, deallocate_count);
}
END_TEST

START_TEST(should_ignore_message_if_allocate_callback_is_null)
{
    init_state();

    message_callbacks.on_publish_allocate_payload = NULL;
    state.internal.header.qos = 1;
    do_decode_buffer("\x00\x03TOP\x02\x06PAY", 10);

    ck_assert(!publish);
    ck_assert_str_eq("", message_received);

    ck_assert_int_eq(0, allocate_topic_count);
    ck_assert_int_eq(0, allocate_payload_count);
    ck_assert_int_eq(0, deallocate_count);
}
END_TEST

START_TEST(should_ignore_message_if_publish_callback_is_null)
{
    init_state();

    message_callbacks.on_publish = NULL;
    state.internal.header.qos = 1;
    do_decode_buffer("\x00\x03TOP\x02\x06PAY", 10);

    ck_assert(!publish);
    ck_assert_str_eq("", message_received);

    ck_assert_int_eq(0, allocate_topic_count);
    ck_assert_int_eq(0, allocate_payload_count);
    ck_assert_int_eq(0, deallocate_count);
}
END_TEST

START_TEST(should_deallocate_publish_if_decode_fails)
{
    init_state();

    state.internal.header.qos = 1;
    message_callbacks.on_publish_allocate_topic =
        &test_on_publish_allocate_topic_fail;
    state.internal.header.remaining_length = 10;
    do_decode((u8) '\x00', LMQTT_DECODE_CONTINUE);
    do_decode((u8) '\x03', LMQTT_DECODE_CONTINUE);
    do_decode((u8) 'T', LMQTT_DECODE_CONTINUE);
    do_decode((u8) 'O', LMQTT_DECODE_ERROR);

    ck_assert_str_eq("", message_received);

    ck_assert_int_eq(1, allocate_topic_count);
    ck_assert_int_eq(0, allocate_payload_count);
    ck_assert_int_eq(1, deallocate_count);
}
END_TEST

START_TEST(should_fail_if_id_set_is_full)
{
    unsigned i;
    char buf[6] = { 0, 1, 'X', 0, 0, 'X' };

    init_state();

    for (i = 0; i < LMQTT_ID_LIST_SIZE; i++) {
        state.internal.header.qos = 2;
        state.internal.packet_id = 0;
        buf[3] = i >> 8;
        buf[4] = i & 0xff;
        do_decode_buffer(buf, 6);
    }

    state.internal.header.qos = 2;
    state.internal.packet_id = 0;
    state.internal.remain_buf_pos = 0;
    state.internal.header.remaining_length = 6;
    state.internal.ignore_publish = 0;
    memset(&state.internal.publish, 0, sizeof(state.internal.publish));

    do_decode(0, LMQTT_DECODE_CONTINUE);
    do_decode(1, LMQTT_DECODE_CONTINUE);
    do_decode((u8) 'X', LMQTT_DECODE_CONTINUE);
    do_decode(0xff, LMQTT_DECODE_CONTINUE);
    do_decode(0xff, LMQTT_DECODE_CONTINUE);
    do_decode((u8) 'X', LMQTT_DECODE_ERROR);
}
END_TEST

START_TCASE("Rx buffer decode publish")
{
    ADD_TEST(should_decode_one_byte_topic_and_payload);
    ADD_TEST(should_decode_long_topic_and_payload);
    ADD_TEST(should_decode_empty_topic);
    ADD_TEST(should_decode_invalid_remaining_length);
    ADD_TEST(should_decode_empty_payload);
    ADD_TEST(should_not_reply_to_publish_with_qos_0);
    ADD_TEST(should_reply_to_publish_with_qos_1);
    ADD_TEST(should_reply_to_publish_with_qos_2);
    ADD_TEST(should_call_callback_multiple_times_with_qos_1);
    ADD_TEST(should_not_call_callback_multiple_times_with_qos_2);
    ADD_TEST(should_call_allocate_callbacks);
    ADD_TEST(should_ignore_message_if_topic_is_ignored);
    ADD_TEST(should_ignore_message_if_payload_is_ignored);
    ADD_TEST(should_ignore_message_if_allocate_callback_is_null);
    ADD_TEST(should_ignore_message_if_publish_callback_is_null);
    ADD_TEST(should_deallocate_publish_if_decode_fails);
    ADD_TEST(should_fail_if_id_set_is_full);
}
END_TCASE
