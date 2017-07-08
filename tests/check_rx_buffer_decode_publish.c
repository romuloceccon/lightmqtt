#include "check_lightmqtt.h"
#include <stdio.h>

#define ENTRY_COUNT 16
#define ID_SET_SIZE 16

static lmqtt_rx_buffer_t state;
static lmqtt_store_t store;
static lmqtt_message_callbacks_t message_callbacks;
static int kind;
static lmqtt_store_value_t value;
static lmqtt_store_entry_t entries[ENTRY_COUNT];
static lmqtt_packet_id_t id_set_items[ID_SET_SIZE];
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
    sprintf(message_received, "topic: %.*s, payload: %.*s",
        (int) publish->topic.len, publish->topic.buf,
        (int) publish->payload.len, publish->payload.buf);
    return 1;
}

static lmqtt_allocate_result_t test_on_publish_allocate_topic(void *data,
    lmqtt_publish_t *publish, size_t len)
{
    if (allocate_topic_result == LMQTT_ALLOCATE_SUCCESS) {
        publish->topic.len = len;
        publish->topic.buf = topic;
    }
    allocate_topic_count++;
    return allocate_topic_result;
}

static lmqtt_allocate_result_t test_on_publish_allocate_payload(void *data,
    lmqtt_publish_t *publish, size_t len)
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

static lmqtt_io_result_t test_write_fail(void *data, void *buf, size_t len,
    size_t *bytes_w, int *os_error)
{
    lmqtt_string_t *str = data;
    if (str->internal.pos >= 1) {
        *bytes_w = 0;
        return LMQTT_IO_ERROR;
    } else {
        *bytes_w = len;
        return LMQTT_IO_SUCCESS;
    }
}

static lmqtt_allocate_result_t test_on_publish_allocate_topic_fail(void *data,
    lmqtt_publish_t *publish, size_t len)
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
    memset(id_set_items, 0, sizeof(id_set_items));
    memset(message_received, 0, sizeof(message_received));
    state.store = &store;
    state.message_callbacks = &message_callbacks;
    state.id_set.items = id_set_items;
    state.id_set.capacity = ID_SET_SIZE;
    state.internal.header.qos = 2;
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

#define DECODE_PUBLISH(b, exp_res, exp_cnt) \
    do { \
        size_t cnt; \
        int res; \
        unsigned char tmp[1]; \
        lmqtt_decode_bytes_t bytes; \
        tmp[0] = (unsigned char) (b); \
        bytes.buf_len = 1; \
        bytes.buf = tmp; \
        bytes.bytes_written = &cnt; \
        res = rx_buffer_decode_publish(&state, &bytes); \
        ck_assert_int_eq((exp_res), res); \
        ck_assert_uint_eq((exp_cnt), cnt); \
        state.internal.remain_buf_pos += (exp_cnt); \
    } while(0)

#define DECODE_PUBLISH_FINISHED(b) DECODE_PUBLISH((b), LMQTT_DECODE_FINISHED, 1)
#define DECODE_PUBLISH_CONTINUE(b) DECODE_PUBLISH((b), LMQTT_DECODE_CONTINUE, 1)
#define DECODE_PUBLISH_ERR_INV(b) DECODE_PUBLISH((b), LMQTT_DECODE_ERROR, 0)
#define DECODE_PUBLISH_ERR_FULL(b) DECODE_PUBLISH((b), LMQTT_DECODE_ERROR, 1)

#define DECODE_PUBLISH_MULTI(buf_p, len, exp_res, exp_cnt) \
    do { \
        size_t cnt; \
        int res; \
        lmqtt_decode_bytes_t bytes; \
        bytes.buf_len = (len); \
        bytes.buf = (unsigned char *) (buf_p); \
        bytes.bytes_written = &cnt; \
        res = rx_buffer_decode_publish(&state, &bytes); \
        ck_assert_int_eq((exp_res), res); \
        ck_assert_uint_eq((exp_cnt), cnt); \
        state.internal.remain_buf_pos += (exp_cnt); \
    } while(0)

static void do_decode_buffer(char *buf, size_t len)
{
    int i;

    state.internal.remain_buf_pos = 0;
    state.internal.header.remaining_length = len;
    state.internal.packet_id = 0;
    state.internal.ignore_publish = 0;
    memset(&state.internal.publish, 0, sizeof(state.internal.publish));

    for (i = 0; i < len - 1; i++)
        DECODE_PUBLISH_CONTINUE(buf[i]);

    DECODE_PUBLISH_FINISHED(buf[len - 1]);
}

START_TEST(should_decode_one_byte_topic_and_payload_with_qos_0)
{
    init_state();
    state.internal.header.qos = 0;

    do_decode_buffer("\x00\x01xx", 4);

    ck_assert_uint_eq(1, state.internal.topic_len);
    ck_assert_uint_eq(0, state.internal.packet_id);
}
END_TEST

START_TEST(should_decode_one_byte_topic_and_payload_with_qos_2)
{
    init_state();
    state.internal.header.qos = 2;

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

    DECODE_PUBLISH_CONTINUE(2);
    DECODE_PUBLISH_CONTINUE(3);
    for (i = 0; i < 0x203; i++)
        DECODE_PUBLISH_CONTINUE('x');
    DECODE_PUBLISH_CONTINUE(3);
    DECODE_PUBLISH_CONTINUE(254);
    DECODE_PUBLISH_CONTINUE('a');
    DECODE_PUBLISH_FINISHED('b');

    ck_assert_uint_eq(0x203, state.internal.topic_len);
    ck_assert_uint_eq(0x3fe, state.internal.packet_id);
}
END_TEST

START_TEST(should_decode_empty_topic)
{
    init_state();

    state.internal.header.remaining_length = 10;

    DECODE_PUBLISH_CONTINUE(0);
    DECODE_PUBLISH_ERR_INV(0);
}
END_TEST

START_TEST(should_decode_multiple_bytes_at_once)
{
    char *msg = "\x00\x03TOP\x02\x06PAY";

    init_state();

    state.internal.header.remaining_length = 10;

    DECODE_PUBLISH_MULTI(&msg[0], 5, LMQTT_DECODE_CONTINUE, 1);
    DECODE_PUBLISH_MULTI(&msg[1], 4, LMQTT_DECODE_CONTINUE, 1);
    DECODE_PUBLISH_MULTI(&msg[2], 3, LMQTT_DECODE_CONTINUE, 3);
    DECODE_PUBLISH_MULTI(&msg[5], 5, LMQTT_DECODE_CONTINUE, 1);
    DECODE_PUBLISH_MULTI(&msg[6], 4, LMQTT_DECODE_CONTINUE, 1);
    DECODE_PUBLISH_MULTI(&msg[7], 3, LMQTT_DECODE_FINISHED, 3);
}
END_TEST

START_TEST(should_decode_multiple_bytes_after_partial_decode)
{
    char *msg = "\x00\x03TOP\x02\x06PAY";

    init_state();

    state.internal.header.remaining_length = 10;

    DECODE_PUBLISH_MULTI(&msg[0], 1, LMQTT_DECODE_CONTINUE, 1);
    DECODE_PUBLISH_MULTI(&msg[1], 1, LMQTT_DECODE_CONTINUE, 1);
    DECODE_PUBLISH_MULTI(&msg[2], 1, LMQTT_DECODE_CONTINUE, 1);
    DECODE_PUBLISH_MULTI(&msg[3], 7, LMQTT_DECODE_CONTINUE, 2);
}
END_TEST

START_TEST(should_decode_buffer_longer_than_topic_length)
{
    char *msg = "\x00\x03TOP\x02\x06PAY";

    init_state();

    state.internal.header.remaining_length = 10;

    DECODE_PUBLISH_MULTI(&msg[0], 10, LMQTT_DECODE_CONTINUE, 1);
    DECODE_PUBLISH_MULTI(&msg[1], 9, LMQTT_DECODE_CONTINUE, 1);
    DECODE_PUBLISH_MULTI(&msg[2], 8, LMQTT_DECODE_CONTINUE, 3);
}
END_TEST

START_TEST(should_decode_invalid_remaining_length)
{
    init_state();

    state.internal.header.remaining_length = 8;

    DECODE_PUBLISH_CONTINUE(0);
    DECODE_PUBLISH_ERR_INV(5);
}
END_TEST

START_TEST(should_decode_malformed_publish_with_qos_2)
{
    init_state();

    /* publish with QoS 2 must have at least 5 bytes */
    state.internal.header.remaining_length = 4;

    DECODE_PUBLISH_CONTINUE(0);
    DECODE_PUBLISH_ERR_INV(1);
}
END_TEST

START_TEST(should_decode_empty_payload_with_qos_0)
{
    init_state();

    state.internal.header.qos = 0;
    do_decode_buffer("\x00\x03xxx", 5);
}
END_TEST

START_TEST(should_decode_empty_payload_with_qos_2)
{
    init_state();

    state.internal.header.qos = 2;
    do_decode_buffer("\x00\x03xxx\x00\x01", 7);
}
END_TEST

START_TEST(should_not_reply_to_publish_with_qos_0)
{
    init_state();

    state.internal.header.qos = 0;
    do_decode_buffer("\x00\x01XX", 4);

    ck_assert_int_eq(0, lmqtt_store_peek(&store, &kind, &value));
}
END_TEST

START_TEST(should_reply_to_publish_with_qos_1)
{
    init_state();

    state.internal.header.qos = 1;
    do_decode_buffer("\x00\x01X\x02\x05X", 6);

    ck_assert_int_eq(1, lmqtt_store_peek(&store, &kind, &value));
    ck_assert_int_eq(LMQTT_KIND_PUBACK, kind);
    ck_assert_int_eq(0x0205, value.packet_id);
}
END_TEST

START_TEST(should_reply_to_publish_with_qos_2)
{
    init_state();

    state.internal.header.qos = 2;
    do_decode_buffer("\x00\x01X\x02\x05X", 6);

    ck_assert_int_eq(1, lmqtt_store_peek(&store, &kind, &value));
    ck_assert_int_eq(LMQTT_KIND_PUBREC, kind);
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

START_TEST(should_ignore_multiple_bytes_if_topic_or_payload_are_ignored)
{
    char *msg = "\x00\x03TOP\x02\x06PAY";

    init_state();

    allocate_topic_result = LMQTT_ALLOCATE_IGNORE;
    allocate_payload_result = LMQTT_ALLOCATE_IGNORE;
    state.internal.header.remaining_length = 10;

    DECODE_PUBLISH_MULTI(&msg[0], 10, LMQTT_DECODE_CONTINUE, 1);
    DECODE_PUBLISH_MULTI(&msg[1], 9, LMQTT_DECODE_CONTINUE, 1);
    DECODE_PUBLISH_MULTI(&msg[2], 8, LMQTT_DECODE_CONTINUE, 3);
    DECODE_PUBLISH_MULTI(&msg[5], 5, LMQTT_DECODE_CONTINUE, 1);
    DECODE_PUBLISH_MULTI(&msg[6], 4, LMQTT_DECODE_CONTINUE, 1);
    DECODE_PUBLISH_MULTI(&msg[7], 3, LMQTT_DECODE_FINISHED, 3);
}
END_TEST

START_TEST(should_deallocate_publish_if_decode_fails)
{
    init_state();

    state.internal.header.qos = 1;
    message_callbacks.on_publish_allocate_topic =
        &test_on_publish_allocate_topic_fail;
    state.internal.header.remaining_length = 10;
    DECODE_PUBLISH_CONTINUE(0);
    DECODE_PUBLISH_CONTINUE(3);
    DECODE_PUBLISH_CONTINUE('T');
    DECODE_PUBLISH_ERR_INV('O');

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

    for (i = 0; i < ID_SET_SIZE; i++) {
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

    DECODE_PUBLISH_CONTINUE(0);
    DECODE_PUBLISH_CONTINUE(1);
    DECODE_PUBLISH_CONTINUE('X');
    DECODE_PUBLISH_CONTINUE(0xff);
    DECODE_PUBLISH_CONTINUE(0xff);
    DECODE_PUBLISH_ERR_FULL('X');
}
END_TEST

START_TCASE("Rx buffer decode publish")
{
    ADD_TEST(should_decode_one_byte_topic_and_payload_with_qos_0);
    ADD_TEST(should_decode_one_byte_topic_and_payload_with_qos_2);
    ADD_TEST(should_decode_long_topic_and_payload);
    ADD_TEST(should_decode_empty_topic);
    ADD_TEST(should_decode_multiple_bytes_at_once);
    ADD_TEST(should_decode_multiple_bytes_after_partial_decode);
    ADD_TEST(should_decode_buffer_longer_than_topic_length);
    ADD_TEST(should_decode_invalid_remaining_length);
    ADD_TEST(should_decode_malformed_publish_with_qos_2);
    ADD_TEST(should_decode_empty_payload_with_qos_0);
    ADD_TEST(should_decode_empty_payload_with_qos_2);
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
    ADD_TEST(should_ignore_multiple_bytes_if_topic_or_payload_are_ignored);
    ADD_TEST(should_deallocate_publish_if_decode_fails);
    ADD_TEST(should_fail_if_id_set_is_full);
}
END_TCASE
