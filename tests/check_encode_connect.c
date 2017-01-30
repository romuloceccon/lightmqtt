#include <check.h>

#include "check_lightmqtt.h"
#include "../src/lightmqtt.c"

#define BUF_PLACEHOLDER 0xcc
#define BYTES_W_PLACEHOLDER -12345
#define CLIENT_ID_PLACEHOLDER 'A'
#define WILL_TOPIC_PLACEHOLDER 'B'
#define WILL_MESSAGE_PLACEHOLDER 'C'
#define USER_NAME_PLACEHOLDER 'D'
#define PASSWORD_PLACEHOLDER 'E'

#define INIT_LMQTT_STRING(field, length, placeholder) \
    connect.field.len = length; \
    connect.field.buf = length > 0 ? field : NULL; \
    memset(field, placeholder, sizeof(field))

#define PREPARE \
    LMqttConnect connect; \
    u8 buf[256]; \
    char client_id[256]; \
    char will_topic[256]; \
    char will_message[256]; \
    char user_name[256]; \
    char password[256]; \
    int bytes_w = BYTES_W_PLACEHOLDER; \
    int res; \
    memset(&connect, 0, sizeof(connect)); \
    memset(buf, BUF_PLACEHOLDER, sizeof(buf)); \
    INIT_LMQTT_STRING(client_id, 1, CLIENT_ID_PLACEHOLDER); \
    INIT_LMQTT_STRING(will_topic, 0, WILL_TOPIC_PLACEHOLDER); \
    INIT_LMQTT_STRING(will_message, 0, WILL_MESSAGE_PLACEHOLDER); \
    INIT_LMQTT_STRING(user_name, 0, USER_NAME_PLACEHOLDER); \
    INIT_LMQTT_STRING(password, 0, PASSWORD_PLACEHOLDER)

START_TEST(should_encode_connect_fixed_header_with_single_byte_remaining_len)
{
    PREPARE;

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

START_TEST(should_encode_connect_keep_alive)
{
    PREPARE;

    connect.keep_alive = 256 + 255;

    res = encode_connect_variable_header(&connect, 0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);

    ck_assert_uint_eq(1,     buf[8]);
    ck_assert_uint_eq(255,   buf[9]);
}
END_TEST

START_TEST(should_encode_connect_will_topic_and_message)
{
    PREPARE;

    connect.will_topic.len = 1;
    connect.will_topic.buf = will_topic;
    connect.will_message.len = 1;
    connect.will_message.buf = will_message;

    res = encode_connect_fixed_header(&connect, 0, buf, 2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(2, bytes_w);
    ck_assert_uint_eq(19, buf[1]);

    res = encode_connect_variable_header(&connect, 0, buf + 2,
        sizeof(buf) - 2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(10, bytes_w);
    ck_assert_uint_eq(0x04, buf[7 + 2]);
}
END_TEST

START_TEST(should_encode_connect_user_name)
{
    PREPARE;

    connect.user_name.len = 1;
    connect.user_name.buf = user_name;

    res = encode_connect_fixed_header(&connect, 0, buf, 2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(2, bytes_w);
    ck_assert_uint_eq(16, buf[1]);

    res = encode_connect_variable_header(&connect, 0, buf + 2,
        sizeof(buf) - 2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(10, bytes_w);
    ck_assert_uint_eq(0x80, buf[7 + 2]);
}
END_TEST

START_TEST(should_encode_connect_password)
{
    PREPARE;

    connect.user_name.len = 1;
    connect.user_name.buf = user_name;
    connect.password.len = 1;
    connect.password.buf = password;

    res = encode_connect_fixed_header(&connect, 0, buf, 2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(2, bytes_w);
    ck_assert_uint_eq(19, buf[1]);

    res = encode_connect_variable_header(&connect, 0, buf + 2,
        sizeof(buf) - 2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(10, bytes_w);
    ck_assert_uint_eq(0xc0, buf[7 + 2]);
}
END_TEST

START_TEST(should_encode_connect_clean_session)
{
    PREPARE;

    connect.client_id.len = 0;
    connect.client_id.buf = NULL;
    connect.clean_session = 1;

    res = encode_connect_fixed_header(&connect, 0, buf, 2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(2, bytes_w);
    ck_assert_uint_eq(12, buf[1]);

    res = encode_connect_variable_header(&connect, 0, buf + 2,
        sizeof(buf) - 2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(10, bytes_w);
    ck_assert_uint_eq(0x02, buf[7 + 2]);
}
END_TEST

START_TEST(should_encode_connect_will_retain)
{
    PREPARE;

    connect.will_retain = 1;
    connect.will_topic.len = 1;
    connect.will_topic.buf = will_topic;
    connect.will_message.len = 1;
    connect.will_message.buf = will_message;

    res = encode_connect_variable_header(&connect, 0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(10, bytes_w);
    ck_assert_uint_eq(0x24, buf[7]);
}
END_TEST

START_TEST(should_encode_connect_qos)
{
    PREPARE;

    connect.qos = 2;

    res = encode_connect_variable_header(&connect, 0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(10, bytes_w);
    ck_assert_uint_eq(0x10, buf[7]);
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
    tcase_add_test(result, should_encode_connect_keep_alive);
    tcase_add_test(result, should_encode_connect_will_topic_and_message);
    tcase_add_test(result, should_encode_connect_user_name);
    tcase_add_test(result, should_encode_connect_password);
    tcase_add_test(result, should_encode_connect_clean_session);
    tcase_add_test(result, should_encode_connect_will_retain);
    tcase_add_test(result, should_encode_connect_qos);
    tcase_add_test(result, should_not_encode_connect_with_insufficient_buffer);

    return result;
}
