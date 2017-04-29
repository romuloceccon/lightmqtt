#include "check_lightmqtt.h"

#define PREPARE \
    int res; \
    lmqtt_rx_buffer_t state; \
    lmqtt_connect_t connect; \
    memset(&state, 0, sizeof(state)); \
    memset(&connect, 0, sizeof(connect)); \
    state.internal.value.value = &connect

START_TEST(should_decode_connack_valid_first_byte)
{
    PREPARE;

    res = rx_buffer_decode_connack(&state, 1);

    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    ck_assert_int_eq(1, connect.response.session_present);
}
END_TEST

START_TEST(should_decode_connack_invalid_first_byte)
{
    PREPARE;

    res = rx_buffer_decode_connack(&state, 3);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(0, connect.response.session_present);
}
END_TEST

START_TEST(should_decode_connack_valid_second_byte)
{
    PREPARE;

    res = rx_buffer_decode_connack(&state, 1);
    state.internal.remain_buf_pos++;
    res = rx_buffer_decode_connack(&state, 3);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(1, connect.response.session_present);
    ck_assert_int_eq(3, connect.response.return_code);
}
END_TEST

START_TEST(should_decode_connack_invalid_second_byte)
{
    PREPARE;

    res = rx_buffer_decode_connack(&state, 1);
    state.internal.remain_buf_pos++;
    res = rx_buffer_decode_connack(&state, 6);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(1, connect.response.session_present);
    ck_assert_int_eq(0, connect.response.return_code);
}
END_TEST

START_TEST(should_not_decode_third_byte)
{
    PREPARE;

    res = rx_buffer_decode_connack(&state, 1);
    state.internal.remain_buf_pos++;
    res = rx_buffer_decode_connack(&state, 3);
    state.internal.remain_buf_pos++;
    res = rx_buffer_decode_connack(&state, 0);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
}
END_TEST

START_TCASE("Rx buffer decode connack")
{
    ADD_TEST(should_decode_connack_valid_first_byte);
    ADD_TEST(should_decode_connack_invalid_first_byte);
    ADD_TEST(should_decode_connack_valid_second_byte);
    ADD_TEST(should_decode_connack_invalid_second_byte);
    ADD_TEST(should_not_decode_third_byte);
}
END_TCASE
