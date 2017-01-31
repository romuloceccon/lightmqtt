#include <check.h>

#include "check_lightmqtt.h"
#include "../src/lightmqtt.c"

#define BYTES_W_PLACEHOLDER -12345
#define BUF_PLACEHOLDER 0xcc
#define STR_PLACEHOLDER 'A'

#define PREPARE \
    LMqttConnect connect; \
    u8 buf[128]; \
    char str[256]; \
    int bytes_w = BYTES_W_PLACEHOLDER; \
    int res; \
    memset(&connect, 0, sizeof(connect)); \
    memset(buf, BUF_PLACEHOLDER, sizeof(buf)); \
    memset(str, STR_PLACEHOLDER, sizeof(str))

START_TEST(should_encode_empty_client_id)
{
    PREPARE;

    res = encode_connect_payload_client_id(&connect, 0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(2, bytes_w);

    ck_assert_uint_eq(0, buf[0]);
    ck_assert_uint_eq(0, buf[1]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[2]);
}
END_TEST

START_TEST(should_encode_non_empty_client_id)
{
    PREPARE;

    connect.client_id.len = 1;
    connect.client_id.buf = str;

    res = encode_connect_payload_client_id(&connect, 0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(3, bytes_w);

    ck_assert_uint_eq(0, buf[0]);
    ck_assert_uint_eq(1, buf[1]);
    ck_assert_uint_eq(STR_PLACEHOLDER, buf[2]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[3]);
}
END_TEST

START_TEST(should_encode_client_id_longer_than_buffer_size)
{
    PREPARE;

    connect.client_id.len = 256;
    connect.client_id.buf = str;

    res = encode_connect_payload_client_id(&connect, 0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_AGAIN, res);
    ck_assert_int_eq(sizeof(buf), bytes_w);

    ck_assert_uint_eq(1, buf[0]);
    ck_assert_uint_eq(0, buf[1]);
    ck_assert_uint_eq(STR_PLACEHOLDER, buf[2]);
    ck_assert_uint_eq(STR_PLACEHOLDER, buf[127]);
}
END_TEST

START_TEST(should_encode_client_id_at_one_byte_buffer)
{
  PREPARE;

  connect.client_id.len = 256;
  connect.client_id.buf = str;

  res = encode_connect_payload_client_id(&connect, 0, buf, 1, &bytes_w);

  ck_assert_int_eq(LMQTT_ENCODE_AGAIN, res);
  ck_assert_int_eq(1, bytes_w);

  ck_assert_uint_eq(1, buf[0]);
  ck_assert_uint_eq(BUF_PLACEHOLDER, buf[1]);
}
END_TEST

START_TEST(should_encode_empty_client_id_at_two_byte_buffer)
{
  PREPARE;

  res = encode_connect_payload_client_id(&connect, 0, buf, 2, &bytes_w);

  ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
  ck_assert_int_eq(2, bytes_w);

  ck_assert_uint_eq(0, buf[0]);
  ck_assert_uint_eq(0, buf[1]);
  ck_assert_uint_eq(BUF_PLACEHOLDER, buf[2]);
}
END_TEST

START_TEST(should_encode_non_empty_client_id_at_two_byte_buffer)
{
  PREPARE;

  connect.client_id.len = 1;
  connect.client_id.buf = str;

  res = encode_connect_payload_client_id(&connect, 0, buf, 2, &bytes_w);

  ck_assert_int_eq(LMQTT_ENCODE_AGAIN, res);
  ck_assert_int_eq(2, bytes_w);

  ck_assert_uint_eq(0, buf[0]);
  ck_assert_uint_eq(1, buf[1]);
  ck_assert_uint_eq(BUF_PLACEHOLDER, buf[2]);
}
END_TEST

START_TEST(should_encode_client_id_starting_at_offset)
{
    PREPARE;

    connect.client_id.len = 128;
    connect.client_id.buf = str;
    str[28] += 1;
    str[28 + 100 - 1] += 1;

    res = encode_connect_payload_client_id(&connect, 30, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(100, bytes_w);

    ck_assert_uint_eq(STR_PLACEHOLDER + 1, buf[0]);
    ck_assert_uint_eq(STR_PLACEHOLDER + 1, buf[99]);
    ck_assert_uint_eq(BUF_PLACEHOLDER,     buf[100]);
}
END_TEST

START_TEST(should_encode_client_id_longer_than_buffer_size_with_offset)
{
    PREPARE;

    connect.client_id.len = 256;
    connect.client_id.buf = str;
    str[50]  += 1;
    str[50 + sizeof(buf) - 1] += 1;

    res = encode_connect_payload_client_id(&connect, 52, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_AGAIN, res);
    ck_assert_int_eq(sizeof(buf), bytes_w);

    ck_assert_uint_eq(STR_PLACEHOLDER + 1, buf[0]);
    ck_assert_uint_eq(STR_PLACEHOLDER + 1, buf[sizeof(buf) - 1]);
}
END_TEST

START_TEST(should_encode_client_id_starting_at_offset_1)
{
    PREPARE;

    connect.client_id.len = 100;
    connect.client_id.buf = str;

    res = encode_connect_payload_client_id(&connect, 1, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(101, bytes_w);

    ck_assert_uint_eq(100, buf[0]);
    ck_assert_uint_eq(STR_PLACEHOLDER, buf[100]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[101]);
}
END_TEST

START_TEST(should_encode_client_id_starting_at_offset_2)
{
    PREPARE;

    connect.client_id.len = 100;
    connect.client_id.buf = str;

    res = encode_connect_payload_client_id(&connect, 2, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(100, bytes_w);

    ck_assert_uint_eq(STR_PLACEHOLDER, buf[99]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[100]);
}
END_TEST

START_TEST(should_encode_empty_user_name)
{
    PREPARE;

    res = encode_connect_payload_user_name(&connect, 0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(0, bytes_w);

    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[0]);
}
END_TEST

START_TEST(should_encode_non_empty_user_name)
{
    PREPARE;

    connect.user_name.len = 1;
    connect.user_name.buf = str;

    res = encode_connect_payload_user_name(&connect, 0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(3, bytes_w);

    ck_assert_uint_eq(0, buf[0]);
    ck_assert_uint_eq(1, buf[1]);
    ck_assert_uint_eq(STR_PLACEHOLDER, buf[2]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[3]);
}
END_TEST

TCase *tcase_encode_connect_payload(void)
{
    TCase *result = tcase_create("Encode connect payload");

    tcase_add_test(result, should_encode_empty_client_id);
    tcase_add_test(result, should_encode_non_empty_client_id);
    tcase_add_test(result, should_encode_client_id_longer_than_buffer_size);
    tcase_add_test(result, should_encode_client_id_at_one_byte_buffer);
    tcase_add_test(result, should_encode_empty_client_id_at_two_byte_buffer);
    tcase_add_test(result, should_encode_non_empty_client_id_at_two_byte_buffer);
    tcase_add_test(result, should_encode_client_id_starting_at_offset);
    tcase_add_test(result, should_encode_client_id_longer_than_buffer_size_with_offset);
    tcase_add_test(result, should_encode_client_id_starting_at_offset_1);
    tcase_add_test(result, should_encode_client_id_starting_at_offset_2);
    tcase_add_test(result, should_encode_empty_user_name);
    tcase_add_test(result, should_encode_non_empty_user_name);

    return result;
}
