#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

#define BYTES_R_PLACEHOLDER -12345

#define PREPARE \
    static int data = 0; \
    LMqttRxBufferState state; \
    u8 buf[64]; \
    int bytes_r = BYTES_R_PLACEHOLDER; \
    int res; \
    memset(&buf, 0, sizeof(buf)); \
    memset(&state, 0, sizeof(state)); \
    state.connect_callback = connect_callback; \
    state.connect_data = &data

void connect_callback(void *data)
{
    *((int *) data) += 1;
}

START_TEST(should_process_complete_rx_buffer)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;

    res = decode_rx_buffer(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(4, bytes_r);

    ck_assert_int_eq(1, data);
}
END_TEST

START_TEST(should_process_partial_rx_buffer)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;

    res = decode_rx_buffer(&state, buf, 3, &bytes_r);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(3, bytes_r);

    ck_assert_int_eq(0, data);
}
END_TEST

START_TEST(should_decode_rx_buffer_continuation)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;

    res = decode_rx_buffer(&state, buf, 1, &bytes_r);
    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(1, bytes_r);
    ck_assert_int_eq(0, data);

    res = decode_rx_buffer(&state, buf + 1, 3, &bytes_r);
    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(3, bytes_r);
    ck_assert_int_eq(1, data);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_invalid_header)
{
    PREPARE;

    buf[1] = 2;

    res = decode_rx_buffer(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(1, bytes_r);

    ck_assert_int_eq(0, data);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_invalid_data)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[2] = 0x0f;

    res = decode_rx_buffer(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(3, bytes_r);

    ck_assert_int_eq(0, data);
}
END_TEST

START_TEST(should_not_decode_rx_buffer_after_error)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[2] = 0x0f;

    res = decode_rx_buffer(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(3, bytes_r);

    buf[2] = 0;

    res = decode_rx_buffer(&state, buf + 2, 2, &bytes_r);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(0, bytes_r);
}
END_TEST

START_TEST(should_reset_rx_buffer_after_successful_processing)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;

    res = decode_rx_buffer(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(1, data);

    res = decode_rx_buffer(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(2, data);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_two_packets)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[4] = 0x20;
    buf[5] = 2;

    res = decode_rx_buffer(&state, buf, 8, &bytes_r);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(8, bytes_r);

    ck_assert_int_eq(2, data);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_allowed_null_data)
{
    PREPARE;

    buf[0] = 0xd0;
    buf[2] = 0xd0;
    buf[4] = 0xd0;

    res = decode_rx_buffer(&state, buf, 6, &bytes_r);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(6, bytes_r);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_disallowed_null_data)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 0;
    buf[2] = 0xd0;

    res = decode_rx_buffer(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(2, bytes_r);
}
END_TEST

START_TCASE("Process rx buffer")
{
    ADD_TEST(should_process_complete_rx_buffer);
    ADD_TEST(should_process_partial_rx_buffer);
    ADD_TEST(should_decode_rx_buffer_continuation);
    ADD_TEST(should_decode_rx_buffer_with_invalid_header);
    ADD_TEST(should_decode_rx_buffer_with_invalid_data);
    ADD_TEST(should_not_decode_rx_buffer_after_error);
    ADD_TEST(should_reset_rx_buffer_after_successful_processing);
    ADD_TEST(should_decode_rx_buffer_with_two_packets);
    ADD_TEST(should_decode_rx_buffer_with_allowed_null_data);
    ADD_TEST(should_decode_rx_buffer_with_disallowed_null_data);
}
END_TCASE
