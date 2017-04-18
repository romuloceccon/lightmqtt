#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

static void do_decode(lmqtt_rx_buffer_t *state, u8 val,
    lmqtt_decode_result_t exp)
{
    int res = rx_buffer_decode_publish(state, val);
    state->internal.remain_buf_pos++;
    ck_assert_int_eq(exp, res);
}

START_TEST(should_decode_one_byte_topic_and_payload)
{
    lmqtt_rx_buffer_t state;
    memset(&state, 0, sizeof(state));
    state.internal.header.remaining_length = 6;

    do_decode(&state, 0, LMQTT_DECODE_CONTINUE);
    do_decode(&state, 1, LMQTT_DECODE_CONTINUE);
    do_decode(&state, (u8) 'x', LMQTT_DECODE_CONTINUE);
    do_decode(&state, 2, LMQTT_DECODE_CONTINUE);
    do_decode(&state, 255, LMQTT_DECODE_CONTINUE);
    do_decode(&state, (u8) 'x', LMQTT_DECODE_FINISHED);

    ck_assert_uint_eq(1, state.internal.topic_len);
    ck_assert_uint_eq(0x2ff, state.internal.packet_id);
}
END_TEST

START_TEST(should_decode_long_topic_and_payload)
{
    int i;
    lmqtt_rx_buffer_t state;
    memset(&state, 0, sizeof(state));
    state.internal.header.remaining_length = 0x203 + 6;

    do_decode(&state, 2, LMQTT_DECODE_CONTINUE);
    do_decode(&state, 3, LMQTT_DECODE_CONTINUE);
    for (i = 0; i < 0x203; i++)
        do_decode(&state, (u8) 'x', LMQTT_DECODE_CONTINUE);
    do_decode(&state, 3, LMQTT_DECODE_CONTINUE);
    do_decode(&state, 254, LMQTT_DECODE_CONTINUE);
    do_decode(&state, (u8) 'a', LMQTT_DECODE_CONTINUE);
    do_decode(&state, (u8) 'b', LMQTT_DECODE_FINISHED);

    ck_assert_uint_eq(0x203, state.internal.topic_len);
    ck_assert_uint_eq(0x3fe, state.internal.packet_id);
}
END_TEST

START_TEST(should_decode_empty_topic)
{
    lmqtt_rx_buffer_t state;
    memset(&state, 0, sizeof(state));
    state.internal.header.remaining_length = 10;

    do_decode(&state, 0, LMQTT_DECODE_CONTINUE);
    do_decode(&state, 0, LMQTT_DECODE_ERROR);
}
END_TEST

START_TEST(should_decode_invalid_remaining_length)
{
    lmqtt_rx_buffer_t state;
    memset(&state, 0, sizeof(state));
    state.internal.header.remaining_length = 8;

    do_decode(&state, 0, LMQTT_DECODE_CONTINUE);
    do_decode(&state, 5, LMQTT_DECODE_ERROR);
}
END_TEST

START_TEST(should_decode_empty_payload)
{
    lmqtt_rx_buffer_t state;
    memset(&state, 0, sizeof(state));
    state.internal.header.remaining_length = 7;

    do_decode(&state, 0, LMQTT_DECODE_CONTINUE);
    do_decode(&state, 3, LMQTT_DECODE_CONTINUE);
    do_decode(&state, (u8) 'x', LMQTT_DECODE_CONTINUE);
    do_decode(&state, (u8) 'x', LMQTT_DECODE_CONTINUE);
    do_decode(&state, (u8) 'x', LMQTT_DECODE_CONTINUE);
    do_decode(&state, 0, LMQTT_DECODE_CONTINUE);
    do_decode(&state, 1, LMQTT_DECODE_FINISHED);
}
END_TEST

START_TCASE("Rx buffer decode publish")
{
    ADD_TEST(should_decode_one_byte_topic_and_payload);
    ADD_TEST(should_decode_long_topic_and_payload);
    ADD_TEST(should_decode_empty_topic);
    ADD_TEST(should_decode_invalid_remaining_length);
    ADD_TEST(should_decode_empty_payload);
}
END_TCASE
