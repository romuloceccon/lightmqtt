#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

#define BYTES_W_PLACEHOLDER -12345
#define BUF_PLACEHOLDER 0xcc
#define STR_PLACEHOLDER 'A'
#define STR_CB_PLACEHOLDER 'B'

#define PREPARE \
    lmqtt_connect_t connect; \
    lmqtt_store_value_t value; \
    lmqtt_encode_buffer_t encode_buffer; \
    u8 buf[128]; \
    char str[256]; \
    test_buffer_t read_buf; \
    int bytes_w = BYTES_W_PLACEHOLDER; \
    int res; \
    memset(&connect, 0, sizeof(connect)); \
    memset(&value, 0, sizeof(value)); \
    memset(&encode_buffer, 0, sizeof(encode_buffer)); \
    memset(buf, BUF_PLACEHOLDER, sizeof(buf)); \
    memset(str, STR_PLACEHOLDER, sizeof(str)); \
    memset(&read_buf, 0, sizeof(read_buf)); \
    memset(&read_buf.buf, STR_CB_PLACEHOLDER, sizeof(read_buf.buf)); \
    value.value = &connect

static lmqtt_read_result_t string_read(void *data, u8 *buf, int buf_len,
    int *bytes_written)
{
    test_buffer_t *test_buffer = (test_buffer_t *) data;
    int count = test_buffer->available_len - test_buffer->pos;
    if (count > buf_len)
        count = buf_len;

    memcpy(buf, test_buffer->buf + test_buffer->pos, count);
    test_buffer->pos += count;
    *bytes_written = count;

    return count == 0 && test_buffer->pos < test_buffer->len ?
        LMQTT_READ_WOULD_BLOCK : LMQTT_READ_SUCCESS;
}

static lmqtt_read_result_t string_read_fail_again(void *data, u8 *buf,
    int buf_len, int *bytes_written)
{
    *bytes_written = 1;
    return LMQTT_READ_WOULD_BLOCK;
}

static lmqtt_read_result_t string_read_fail_error(void *data, u8 *buf,
    int buf_len, int *bytes_written)
{
    *bytes_written = 1;
    return LMQTT_READ_ERROR;
}

START_TEST(should_encode_empty_client_id)
{
    PREPARE;

    res = connect_encode_payload_client_id(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

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

    res = connect_encode_payload_client_id(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

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

    res = connect_encode_payload_client_id(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_CONTINUE, res);
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

    res = connect_encode_payload_client_id(&value, &encode_buffer, 0, buf,
        1, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_CONTINUE, res);
    ck_assert_int_eq(1, bytes_w);

    ck_assert_uint_eq(1, buf[0]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[1]);
}
END_TEST

START_TEST(should_encode_empty_client_id_at_two_byte_buffer)
{
    PREPARE;

    res = connect_encode_payload_client_id(&value, &encode_buffer, 0, buf,
        2, &bytes_w);

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

    res = connect_encode_payload_client_id(&value, &encode_buffer, 0, buf,
        2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_CONTINUE, res);
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

    res = connect_encode_payload_client_id(&value, &encode_buffer, 30, buf,
        sizeof(buf), &bytes_w);

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

    res = connect_encode_payload_client_id(&value, &encode_buffer, 52, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_CONTINUE, res);
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

    res = connect_encode_payload_client_id(&value, &encode_buffer, 1, buf,
        sizeof(buf), &bytes_w);

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

    res = connect_encode_payload_client_id(&value, &encode_buffer, 2, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(100, bytes_w);

    ck_assert_uint_eq(STR_PLACEHOLDER, buf[99]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[100]);
}
END_TEST

START_TEST(should_encode_empty_user_name)
{
    PREPARE;

    res = connect_encode_payload_user_name(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

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

    res = connect_encode_payload_user_name(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(3, bytes_w);

    ck_assert_uint_eq(0, buf[0]);
    ck_assert_uint_eq(1, buf[1]);
    ck_assert_uint_eq(STR_PLACEHOLDER, buf[2]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[3]);
}
END_TEST

START_TEST(should_encode_empty_user_name_at_zero_byte_buffer)
{
    PREPARE;

    res = connect_encode_payload_user_name(&value, &encode_buffer, 0, buf,
        0, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(0, bytes_w);

    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[0]);
}
END_TEST

START_TEST(should_encode_user_name_with_callback)
{
    PREPARE;

    connect.user_name.len = 10;
    connect.user_name.data = &read_buf;
    connect.user_name.read = string_read;

    read_buf.len = 10;
    read_buf.available_len = 10;

    res = connect_encode_payload_user_name(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(12, bytes_w);

    ck_assert_uint_eq(0, buf[0]);
    ck_assert_uint_eq(10, buf[1]);
    ck_assert_uint_eq(STR_CB_PLACEHOLDER, buf[11]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[12]);
}
END_TEST

START_TEST(should_encode_user_name_with_read_buf_longer_than_buffer_size)
{
    PREPARE;

    connect.user_name.len = sizeof(buf);
    connect.user_name.data = &read_buf;
    connect.user_name.read = string_read;

    read_buf.len = sizeof(buf);
    read_buf.available_len = sizeof(buf);

    res = connect_encode_payload_user_name(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_CONTINUE, res);
    ck_assert_int_eq(sizeof(buf), bytes_w);

    ck_assert_uint_eq(0, buf[0]);
    ck_assert_uint_eq(sizeof(buf), buf[1]);
    ck_assert_uint_eq(STR_CB_PLACEHOLDER, buf[sizeof(buf) - 1]);

    res = connect_encode_payload_user_name(&value, &encode_buffer,
        sizeof(buf), buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(2, bytes_w);

    ck_assert_uint_eq(STR_CB_PLACEHOLDER, buf[0]);
    ck_assert_uint_eq(STR_CB_PLACEHOLDER, buf[1]);
}
END_TEST

START_TEST(should_encode_user_name_with_fewer_bytes_available_than_string_size)
{
    PREPARE;

    connect.user_name.len = 10;
    connect.user_name.data = &read_buf;
    connect.user_name.read = string_read;

    read_buf.len = 10;
    read_buf.available_len = 5;

    res = connect_encode_payload_user_name(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_CONTINUE, res);
    ck_assert_int_eq(7, bytes_w);
    ck_assert_ptr_eq(NULL, encode_buffer.blocking_str);

    ck_assert_uint_eq(0, buf[0]);
    ck_assert_uint_eq(10, buf[1]);
    ck_assert_uint_eq(STR_CB_PLACEHOLDER, buf[6]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[7]);

    res = connect_encode_payload_user_name(&value, &encode_buffer, 7, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_WOULD_BLOCK, res);
    ck_assert_int_eq(0, bytes_w);
    ck_assert_ptr_eq(&connect.user_name, encode_buffer.blocking_str);

    read_buf.available_len = 10;

    res = connect_encode_payload_user_name(&value, &encode_buffer, 7, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(5, bytes_w);
    ck_assert_ptr_eq(NULL, encode_buffer.blocking_str);
}
END_TEST

START_TEST(should_fail_if_read_buf_returns_fewer_bytes_than_expected)
{
    PREPARE;

    connect.user_name.len = 10;
    connect.user_name.data = &read_buf;
    connect.user_name.read = string_read;

    read_buf.len = 5;
    read_buf.available_len = 5;

    res = connect_encode_payload_user_name(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_CONTINUE, res);
    ck_assert_int_eq(7, bytes_w);

    res = connect_encode_payload_user_name(&value, &encode_buffer, 7, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_ERROR, res);
    ck_assert_int_eq(0, bytes_w);
}
END_TEST

START_TEST(should_fail_if_read_buf_returns_again_with_non_zero_byte_count)
{
    PREPARE;

    connect.user_name.len = 10;
    connect.user_name.data = &read_buf;
    connect.user_name.read = string_read_fail_again;

    res = connect_encode_payload_user_name(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_ERROR, res);
    ck_assert_int_eq(3, bytes_w);
}
END_TEST

START_TEST(should_fail_if_read_buf_returns_error_with_non_zero_byte_count)
{
    PREPARE;

    connect.user_name.len = 10;
    connect.user_name.data = &read_buf;
    connect.user_name.read = string_read_fail_error;

    res = connect_encode_payload_user_name(&value, &encode_buffer, 0, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_ERROR, res);
    ck_assert_int_eq(3, bytes_w);
}
END_TEST

START_TCASE("Encode connect payload")
{
    ADD_TEST(should_encode_empty_client_id);
    ADD_TEST(should_encode_non_empty_client_id);
    ADD_TEST(should_encode_client_id_longer_than_buffer_size);
    ADD_TEST(should_encode_client_id_at_one_byte_buffer);
    ADD_TEST(should_encode_empty_client_id_at_two_byte_buffer);
    ADD_TEST(should_encode_non_empty_client_id_at_two_byte_buffer);
    ADD_TEST(should_encode_client_id_starting_at_offset);
    ADD_TEST(should_encode_client_id_longer_than_buffer_size_with_offset);
    ADD_TEST(should_encode_client_id_starting_at_offset_1);
    ADD_TEST(should_encode_client_id_starting_at_offset_2);
    ADD_TEST(should_encode_empty_user_name);
    ADD_TEST(should_encode_non_empty_user_name);
    ADD_TEST(should_encode_empty_user_name_at_zero_byte_buffer);
    ADD_TEST(should_encode_user_name_with_callback);
    ADD_TEST(should_encode_user_name_with_read_buf_longer_than_buffer_size);
    ADD_TEST(should_encode_user_name_with_fewer_bytes_available_than_string_size);
    ADD_TEST(should_fail_if_read_buf_returns_fewer_bytes_than_expected);
    ADD_TEST(should_fail_if_read_buf_returns_again_with_non_zero_byte_count);
    ADD_TEST(should_fail_if_read_buf_returns_error_with_non_zero_byte_count);
}
END_TCASE
