#include "check_lightmqtt.h"

#define PREPARE \
    int res; \
    lmqtt_rx_buffer_t state; \
    lmqtt_connect_t connect; \
    memset(&state, 0, sizeof(state)); \
    memset(&connect, 0, sizeof(connect)); \
    state.internal.value.value = &connect

#define DECODE_CONNACK(b, exp_cnt) \
    do { \
        size_t cnt; \
        unsigned char buf[1]; \
        lmqtt_decode_bytes_t bytes; \
        buf[0] = (b); \
        bytes.buf_len = 1; \
        bytes.buf = buf; \
        bytes.bytes_written = &cnt; \
        res = rx_buffer_decode_connack(&state, &bytes); \
        ck_assert_uint_eq((exp_cnt), cnt); \
    } while(0)

#define DECODE_CONNACK_OK(b) DECODE_CONNACK((b), 1)
#define DECODE_CONNACK_ERR(b) DECODE_CONNACK((b), 0)

START_TEST(should_decode_connack_valid_first_byte)
{
    PREPARE;

    DECODE_CONNACK_OK(1);

    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    ck_assert_int_eq(1, connect.response.session_present);
}
END_TEST

START_TEST(should_decode_connack_invalid_first_byte)
{
    PREPARE;

    DECODE_CONNACK_ERR(3);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(0, connect.response.session_present);
}
END_TEST

START_TEST(should_decode_connack_valid_second_byte)
{
    PREPARE;

    DECODE_CONNACK_OK(1);
    state.internal.remain_buf_pos++;
    DECODE_CONNACK_OK(3);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(1, connect.response.session_present);
    ck_assert_int_eq(3, connect.response.return_code);
}
END_TEST

START_TEST(should_decode_connack_invalid_second_byte)
{
    PREPARE;

    DECODE_CONNACK_OK(1);
    state.internal.remain_buf_pos++;
    DECODE_CONNACK_ERR(6);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(1, connect.response.session_present);
    ck_assert_int_eq(0, connect.response.return_code);
}
END_TEST

START_TEST(should_not_decode_third_byte)
{
    PREPARE;

    DECODE_CONNACK_OK(1);
    state.internal.remain_buf_pos++;
    DECODE_CONNACK_OK(3);
    state.internal.remain_buf_pos++;
    DECODE_CONNACK_ERR(0);

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
