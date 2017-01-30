#include <stdlib.h>
#include <check.h>

#include "check_lightmqtt.h"
#include "../src/lightmqtt.c"

#define BUF_PLACEHOLDER 0xcc
#define BYTES_W_PLACEHOLDER -12345
#define CLIENT_ID_PLACEHOLDER 'X'

#define PREPARE \
    LMqttConnect connect; \
    u8 buf[256]; \
    char client_id[256]; \
    int bytes_w = BYTES_W_PLACEHOLDER; \
    int res; \
    memset(&connect, 0, sizeof(connect)); \
    memset(buf, BUF_PLACEHOLDER, sizeof(buf)); \
    memset(client_id, CLIENT_ID_PLACEHOLDER, sizeof(client_id))

START_TEST(should_encode_connect_fixed_header_with_single_byte_remaining_len)
{
    PREPARE;

    connect.client_id.len = 1;
    connect.client_id.buf = client_id;

    res = encode_connect_fixed_header(&connect, 0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(2, bytes_w);

    ck_assert_uint_eq(0x10, buf[0]);
    ck_assert_uint_eq(13,   buf[1]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[2]);
}
END_TEST

START_TEST(should_not_encode_connect_fixed_header_with_insufficient_buffer)
{
    PREPARE;

    connect.client_id.len = 116;
    connect.client_id.buf = client_id;

    res = encode_connect_fixed_header(&connect, 0, buf, 2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_ERROR, res);
    ck_assert_int_eq(BYTES_W_PLACEHOLDER, bytes_w);

    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[0]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[1]);
}
END_TEST

START_TEST(should_encode_simple_connect)
{
    PREPARE;

    res = encode_connect_variable_header(&connect, 0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(10, bytes_w);

    ck_assert_uint_eq(0x00, buf[0]);
    ck_assert_uint_eq(0x04, buf[1]);
    ck_assert_uint_eq('M',  buf[2]);
    ck_assert_uint_eq('Q',  buf[3]);
    ck_assert_uint_eq('T',  buf[4]);
    ck_assert_uint_eq('T',  buf[5]);

    ck_assert_uint_eq(0x04,  buf[6]);
    ck_assert_uint_eq(0x00,  buf[7]);
    ck_assert_uint_eq(0x00,  buf[8]);
    ck_assert_uint_eq(0x00,  buf[9]);

    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[10]);
}
END_TEST

START_TEST(should_not_encode_connect_with_insufficient_buffer)
{
    PREPARE;

    res = encode_connect_variable_header(&connect, 0, buf, 9, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_ERROR, res);
    ck_assert_int_eq(BYTES_W_PLACEHOLDER, bytes_w);

    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[0]);
}
END_TEST

TCase *tcase_encode_connect(void)
{
    TCase *result = tcase_create("Encode connect");

    tcase_add_test(result, should_encode_connect_fixed_header_with_single_byte_remaining_len);
    tcase_add_test(result, should_not_encode_connect_fixed_header_with_insufficient_buffer);
    tcase_add_test(result, should_encode_simple_connect);
    tcase_add_test(result, should_not_encode_connect_with_insufficient_buffer);

    return result;
}
