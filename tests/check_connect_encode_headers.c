#include "check_lightmqtt.h"

#define BUF_PLACEHOLDER 0xcc
#define BYTES_W_PLACEHOLDER ((size_t) -12345)
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
    lmqtt_connect_t connect; \
    lmqtt_store_value_t value; \
    lmqtt_encode_buffer_t encode_buffer; \
    unsigned char buf[256]; \
    char client_id[256]; \
    char will_topic[256]; \
    char will_message[256]; \
    char user_name[256]; \
    char password[256]; \
    size_t bytes_w = BYTES_W_PLACEHOLDER; \
    int res; \
    memset(&connect, 0, sizeof(connect)); \
    memset(&value, 0, sizeof(value)); \
    memset(&encode_buffer, 0, sizeof(encode_buffer)); \
    memset(buf, BUF_PLACEHOLDER, sizeof(buf)); \
    memset(&encode_buffer.buf, BUF_PLACEHOLDER, sizeof(encode_buffer.buf)); \
    value.value = &connect; \
    INIT_LMQTT_STRING(client_id, 1, CLIENT_ID_PLACEHOLDER); \
    INIT_LMQTT_STRING(will_topic, 0, WILL_TOPIC_PLACEHOLDER); \
    INIT_LMQTT_STRING(will_message, 0, WILL_MESSAGE_PLACEHOLDER); \
    INIT_LMQTT_STRING(user_name, 0, USER_NAME_PLACEHOLDER); \
    INIT_LMQTT_STRING(password, 0, PASSWORD_PLACEHOLDER)

START_TEST(should_encode_connect_fixed_header_with_single_byte_remaining_len)
{
    PREPARE;

    res = connect_build_fixed_header(&value, &encode_buffer);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);

    ck_assert_uint_eq(2,    encode_buffer.buf_len);
    ck_assert_uint_eq(0x10, encode_buffer.buf[0]);
    ck_assert_uint_eq(13,   encode_buffer.buf[1]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, encode_buffer.buf[2]);
}
END_TEST

START_TEST(should_encode_connect_fixed_header_with_two_byte_remaining_len)
{
    PREPARE;

    connect.client_id.buf = client_id;
    connect.client_id.len = 256;

    res = connect_build_fixed_header(&value, &encode_buffer);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);

    ck_assert_uint_eq(3,    encode_buffer.buf_len);
    ck_assert_uint_eq(0x10, encode_buffer.buf[0]);
    ck_assert_uint_eq(140,  encode_buffer.buf[1]);
    ck_assert_uint_eq(2,    encode_buffer.buf[2]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, encode_buffer.buf[3]);
}
END_TEST

START_TEST(should_encode_connect_variable_header)
{
    PREPARE;

    res = connect_build_variable_header(&value, &encode_buffer);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);

    ck_assert_uint_eq(0x00, encode_buffer.buf[0]);
    ck_assert_uint_eq(0x04, encode_buffer.buf[1]);
    ck_assert_uint_eq('M',  encode_buffer.buf[2]);
    ck_assert_uint_eq('Q',  encode_buffer.buf[3]);
    ck_assert_uint_eq('T',  encode_buffer.buf[4]);
    ck_assert_uint_eq('T',  encode_buffer.buf[5]);

    ck_assert_uint_eq(0x04,  encode_buffer.buf[6]);
    ck_assert_uint_eq(0x00,  encode_buffer.buf[7]);
    ck_assert_uint_eq(0x00,  encode_buffer.buf[8]);
    ck_assert_uint_eq(0x00,  encode_buffer.buf[9]);

    ck_assert_uint_eq(BUF_PLACEHOLDER, encode_buffer.buf[10]);
}
END_TEST

START_TEST(should_encode_connect_keep_alive)
{
    PREPARE;

    connect.keep_alive = 256 + 255;

    res = connect_encode_variable_header(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

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

    res = connect_encode_fixed_header(&value, &encode_buffer, 0, buf,
        2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(2, bytes_w);
    ck_assert_uint_eq(19, buf[1]);

    res = connect_encode_variable_header(&value, &encode_buffer, 0, buf + 2,
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

    res = connect_encode_fixed_header(&value, &encode_buffer, 0, buf,
        2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(2, bytes_w);
    ck_assert_uint_eq(16, buf[1]);

    res = connect_encode_variable_header(&value, &encode_buffer, 0, buf + 2,
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

    res = connect_encode_fixed_header(&value, &encode_buffer, 0, buf,
        2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(2, bytes_w);
    ck_assert_uint_eq(19, buf[1]);

    res = connect_encode_variable_header(&value, &encode_buffer, 0, buf + 2,
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

    res = connect_encode_fixed_header(&value, &encode_buffer, 0, buf,
        2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(2, bytes_w);
    ck_assert_uint_eq(12, buf[1]);

    res = connect_encode_variable_header(&value, &encode_buffer, 0, buf + 2,
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

    res = connect_encode_variable_header(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(10, bytes_w);
    ck_assert_uint_eq(0x24, buf[7]);
}
END_TEST

START_TEST(should_encode_connect_qos)
{
    PREPARE;

    connect.will_qos = LMQTT_QOS_2;

    res = connect_encode_variable_header(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(10, bytes_w);
    ck_assert_uint_eq(0x10, buf[7]);
}
END_TEST

START_TEST(should_encode_connect_with_offset)
{
    PREPARE;

    res = connect_encode_variable_header(&value, &encode_buffer, 0, buf,
        3, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_CONTINUE, res);
    ck_assert_int_eq(3, bytes_w);

    ck_assert_uint_eq(0x00, buf[0]);
    ck_assert_uint_eq(0x04, buf[1]);
    ck_assert_uint_eq('M',  buf[2]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[3]);

    res = connect_encode_variable_header(&value, &encode_buffer, 3, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(7, bytes_w);

    ck_assert_uint_eq('Q',  buf[0]);
    ck_assert_uint_eq('T',  buf[1]);
    ck_assert_uint_eq('T',  buf[2]);

    ck_assert_uint_eq(0x04,  buf[3]);
    ck_assert_uint_eq(0x00,  buf[4]);
    ck_assert_uint_eq(0x00,  buf[5]);
    ck_assert_uint_eq(0x00,  buf[6]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[7]);
}
END_TEST

START_TCASE("Encode connect headers")
{
    ADD_TEST(should_encode_connect_fixed_header_with_single_byte_remaining_len);
    ADD_TEST(should_encode_connect_fixed_header_with_two_byte_remaining_len);
    ADD_TEST(should_encode_connect_variable_header);
    ADD_TEST(should_encode_connect_keep_alive);
    ADD_TEST(should_encode_connect_will_topic_and_message);
    ADD_TEST(should_encode_connect_user_name);
    ADD_TEST(should_encode_connect_password);
    ADD_TEST(should_encode_connect_clean_session);
    ADD_TEST(should_encode_connect_will_retain);
    ADD_TEST(should_encode_connect_qos);
    ADD_TEST(should_encode_connect_with_offset);
}
END_TCASE
