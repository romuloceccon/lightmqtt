#include "check_lightmqtt.h"

#define PREPARE \
    int res; \
    lmqtt_error_t error; \
    int os_error = 0xcccc; \
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
        error = lmqtt_rx_buffer_get_error(&state, &os_error); \
    } while(0)

#define DECODE_CONNACK_OK(b) DECODE_CONNACK((b), 1)
#define DECODE_CONNACK_ERR(b) DECODE_CONNACK((b), 0)
#define DECODE_CONNACK_FAIL(b) DECODE_CONNACK((b), 1)

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
    ck_assert_int_eq(LMQTT_ERROR_DECODE_CONNACK_INVALID_ACKNOWLEDGE_FLAGS,
        error);
}
END_TEST

START_TEST(should_decode_connack_valid_second_byte_success)
{
    PREPARE;

    DECODE_CONNACK_OK(1);
    state.internal.remain_buf_pos++;
    DECODE_CONNACK_OK(0);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(1, connect.response.session_present);
    ck_assert_int_eq(0, error);
}
END_TEST

START_TEST(should_decode_connack_valid_second_byte_failure)
{
    PREPARE;

    DECODE_CONNACK_OK(1);
    state.internal.remain_buf_pos++;
    DECODE_CONNACK_FAIL(5);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(1, connect.response.session_present);
    ck_assert_int_eq(LMQTT_ERROR_CONNACK_NOT_AUTHORIZED, error);
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
    ck_assert_int_eq(LMQTT_ERROR_DECODE_CONNACK_INVALID_RETURN_CODE, error);
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
    ck_assert_int_eq(LMQTT_ERROR_DECODE_CONNACK_INVALID_LENGTH, error);
}
END_TEST

START_TEST(should_decode_buffer_with_multiple_bytes)
{
    size_t cnt;
    const char *buf = "\x01\x00";
    lmqtt_decode_bytes_t bytes;

    PREPARE;

    bytes.buf_len = 2;
    bytes.buf = (unsigned char *) &buf[0];
    bytes.bytes_written = &cnt;

    res = rx_buffer_decode_connack(&state, &bytes);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    ck_assert_uint_eq(1, cnt);
    state.internal.remain_buf_pos++;

    bytes.buf_len = 1;
    bytes.buf = (unsigned char *) &buf[1];

    res = rx_buffer_decode_connack(&state, &bytes);
    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_uint_eq(1, cnt);
}
END_TEST

START_TCASE("Rx buffer decode connack")
{
    ADD_TEST(should_decode_connack_valid_first_byte);
    ADD_TEST(should_decode_connack_invalid_first_byte);
    ADD_TEST(should_decode_connack_valid_second_byte_success);
    ADD_TEST(should_decode_connack_valid_second_byte_failure);
    ADD_TEST(should_decode_connack_invalid_second_byte);
    ADD_TEST(should_not_decode_third_byte);
    ADD_TEST(should_decode_buffer_with_multiple_bytes);
}
END_TCASE
