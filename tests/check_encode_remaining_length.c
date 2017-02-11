#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

#define BUF_PLACEHOLDER 0xcc
#define BYTES_W_PLACEHOLDER -12345

#define PREPARE \
    u8 buf[256]; \
    int bytes_w = BYTES_W_PLACEHOLDER; \
    int res; \
    memset(buf, BUF_PLACEHOLDER, sizeof(buf))

START_TEST(should_encode_minimum_single_byte_remaining_len)
{
    PREPARE;

    res = encode_remaining_length(0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(1, bytes_w);

    ck_assert_uint_eq(0,    buf[0]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[1]);
}
END_TEST

START_TEST(should_encode_maximum_single_byte_remaining_len)
{
    PREPARE;

    res = encode_remaining_length(127, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(1, bytes_w);

    ck_assert_uint_eq(127,  buf[0]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[1]);
}
END_TEST

START_TEST(should_encode_minimum_two_byte_remaining_len)
{
    PREPARE;

    res = encode_remaining_length(128, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(2, bytes_w);

    ck_assert_uint_eq(0X80 | 0, buf[0]);
    ck_assert_uint_eq(       1, buf[1]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[2]);
}
END_TEST

START_TEST(should_encode_maximum_four_byte_remaining_len)
{
    PREPARE;

    res = encode_remaining_length(268435455, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(4, bytes_w);

    ck_assert_uint_eq(0x80 | 127, buf[0]);
    ck_assert_uint_eq(0x80 | 127, buf[1]);
    ck_assert_uint_eq(0x80 | 127, buf[2]);
    ck_assert_uint_eq(       127, buf[3]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[4]);
}
END_TEST

START_TEST(should_not_encode_negative_remaining_len)
{
    PREPARE;

    res = encode_remaining_length(-1, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_ERROR, res);
    ck_assert_int_eq(BYTES_W_PLACEHOLDER, bytes_w);

    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[0]);
}
END_TEST

START_TEST(should_not_encode_remaining_len_greater_than_maximum)
{
    PREPARE;

    res = encode_remaining_length(268435455 + 1, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_ERROR, res);
    ck_assert_int_eq(BYTES_W_PLACEHOLDER, bytes_w);

    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[0]);
}
END_TEST

START_TEST(should_not_encode_remaining_len_greater_than_allowed_by_buffer_len)
{
    PREPARE;

    res = encode_remaining_length(16383 + 1, buf, 2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_ERROR, res);
    ck_assert_int_eq(BYTES_W_PLACEHOLDER, bytes_w);

    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[0]);
}
END_TEST

START_TCASE("Encode remaining length")
{
    ADD_TEST(should_encode_minimum_single_byte_remaining_len);
    ADD_TEST(should_encode_maximum_single_byte_remaining_len);
    ADD_TEST(should_encode_minimum_two_byte_remaining_len);
    ADD_TEST(should_encode_maximum_four_byte_remaining_len);
    ADD_TEST(should_not_encode_negative_remaining_len);
    ADD_TEST(should_not_encode_remaining_len_greater_than_maximum);
    ADD_TEST(should_not_encode_remaining_len_greater_than_allowed_by_buffer_len);
}
END_TCASE
