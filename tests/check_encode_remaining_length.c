#include "check_lightmqtt.h"

#define BUF_PLACEHOLDER 0xcc
#define BYTES_W_PLACEHOLDER ((size_t) -12345)

#define PREPARE \
    unsigned char buf[256]; \
    size_t bytes_w = BYTES_W_PLACEHOLDER; \
    memset(buf, BUF_PLACEHOLDER, sizeof(buf))

START_TEST(should_encode_minimum_single_byte_remaining_len)
{
    PREPARE;

    bytes_w = encode_remaining_length(0, buf);

    ck_assert_int_eq(1, bytes_w);

    ck_assert_uint_eq(0,    buf[0]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[1]);
}
END_TEST

START_TEST(should_encode_maximum_single_byte_remaining_len)
{
    PREPARE;

    bytes_w = encode_remaining_length(127, buf);

    ck_assert_int_eq(1, bytes_w);

    ck_assert_uint_eq(127,  buf[0]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[1]);
}
END_TEST

START_TEST(should_encode_minimum_two_byte_remaining_len)
{
    PREPARE;

    bytes_w = encode_remaining_length(128, buf);

    ck_assert_int_eq(2, bytes_w);

    ck_assert_uint_eq(0X80 | 0, buf[0]);
    ck_assert_uint_eq(       1, buf[1]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[2]);
}
END_TEST

START_TEST(should_encode_maximum_four_byte_remaining_len)
{
    PREPARE;

    bytes_w = encode_remaining_length(268435455, buf);

    ck_assert_int_eq(4, bytes_w);

    ck_assert_uint_eq(0x80 | 127, buf[0]);
    ck_assert_uint_eq(0x80 | 127, buf[1]);
    ck_assert_uint_eq(0x80 | 127, buf[2]);
    ck_assert_uint_eq(       127, buf[3]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[4]);
}
END_TEST

START_TCASE("Encode remaining length")
{
    ADD_TEST(should_encode_minimum_single_byte_remaining_len);
    ADD_TEST(should_encode_maximum_single_byte_remaining_len);
    ADD_TEST(should_encode_minimum_two_byte_remaining_len);
    ADD_TEST(should_encode_maximum_four_byte_remaining_len);
}
END_TCASE
