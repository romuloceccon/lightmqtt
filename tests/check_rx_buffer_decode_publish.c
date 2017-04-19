#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

static lmqtt_rx_buffer_t state;
static lmqtt_store_t store;
static int class;
static lmqtt_store_value_t value;
static lmqtt_publish_t *publish;

static int test_on_publish(void *data, lmqtt_publish_t *publish)
{
    lmqtt_publish_t **dst = (lmqtt_publish_t **) data;
    *dst = publish;
    return 1;
}

static void init_state()
{
    memset(&state, 0, sizeof(state));
    memset(&store, 0, sizeof(store));
    state.store = &store;
    state.on_publish = &test_on_publish;
    state.on_publish_data = &publish;
    store.get_time = &test_time_get;
    publish = NULL;
}

static void do_decode(u8 val, lmqtt_decode_result_t exp)
{
    int res = rx_buffer_decode_publish(&state, val);
    state.internal.remain_buf_pos++;
    ck_assert_int_eq(exp, res);
}

static void do_decode_buffer(char *buf, int len)
{
    int i;

    state.internal.remain_buf_pos = 0;
    state.internal.header.remaining_length = len;
    state.internal.packet_id = 0;

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

    publish = NULL;
    do_decode_buffer("\x00\x01X\x02\x06X", 6);

    ck_assert_ptr_eq(&state.internal.publish, publish);
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
    ADD_TEST(should_fail_if_id_set_is_full);
}
END_TCASE
