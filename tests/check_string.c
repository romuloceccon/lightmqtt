#include "check_lightmqtt.h"

typedef struct {
    unsigned char buf[10];
    int len;
    int pos;
} test_write_buffer_t;

lmqtt_write_result_t test_write(void *data, void *buf, int len, int *bytes_w)
{
    test_write_buffer_t *buffer = data;
    unsigned char *buf_c = buf;
    int i;

    *bytes_w = 0;

    for (i = 0; i < len; i++) {
        if (buffer->pos >= buffer->len)
            return LMQTT_WRITE_WOULD_BLOCK;
        buffer->buf[buffer->pos++] = buf_c[i];
        *bytes_w += 1;
    }

    return LMQTT_WRITE_SUCCESS;
}

lmqtt_write_result_t test_write_fail(void *data, void *buf, int len,
    int *bytes_w)
{
    return LMQTT_WRITE_ERROR;
}

START_TEST(should_put_chars)
{
    char buf[2];
    lmqtt_string_t str;
    lmqtt_string_t *blk;
    int res;

    memset(buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));

    str.buf = buf;
    str.len = sizeof(buf);

    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, string_put(&str, 'a', &blk));
    ck_assert_int_eq(LMQTT_DECODE_FINISHED, string_put(&str, 'b', &blk));
    ck_assert_int_eq('a', buf[0]);
    ck_assert_int_eq('b', buf[1]);
}
END_TEST

START_TEST(should_put_chars_with_callback)
{
    test_write_buffer_t buf;
    lmqtt_string_t str;
    lmqtt_string_t *blk;
    int res;

    memset(&buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));
    str.write = &test_write;
    str.data = &buf;
    str.len = 2;
    buf.len = 2;

    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, string_put(&str, 'a', &blk));
    ck_assert_int_eq(LMQTT_DECODE_FINISHED, string_put(&str, 'b', &blk));
    ck_assert_int_eq('a', buf.buf[0]);
    ck_assert_int_eq('b', buf.buf[1]);
}
END_TEST

START_TEST(should_put_chars_with_blocking_decode)
{
    test_write_buffer_t buf;
    lmqtt_string_t str;
    lmqtt_string_t *blk = &str;
    int res;

    memset(&buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));
    str.write = &test_write;
    str.data = &buf;
    str.len = 2;
    buf.len = 1;

    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, string_put(&str, 'a', &blk));
    ck_assert_ptr_eq(NULL, blk);
    ck_assert_int_eq(LMQTT_DECODE_WOULD_BLOCK, string_put(&str, 'b', &blk));
    ck_assert_ptr_eq(&str, blk);
}
END_TEST

START_TEST(should_put_chars_with_decode_error)
{
    lmqtt_string_t str;
    lmqtt_string_t *blk;
    int res;

    memset(&str, 0, sizeof(str));
    str.write = &test_write_fail;
    str.len = 2;

    ck_assert_int_eq(LMQTT_DECODE_ERROR, string_put(&str, 'a', &blk));
}
END_TEST

START_TEST(should_not_overflow_buffer)
{
    char buf[1];
    lmqtt_string_t str;
    lmqtt_string_t *blk;
    int res;

    memset(buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));

    str.buf = buf;
    str.len = sizeof(buf);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, string_put(&str, 'a', &blk));
    ck_assert_int_eq(LMQTT_DECODE_ERROR, string_put(&str, 'b', &blk));
}
END_TEST

START_TEST(should_encode_non_empty_string_on_zero_length_buffer)
{
    char *buf = "test";
    lmqtt_string_t str;
    lmqtt_string_t *blk;
    int res;
    int bytes_w = -1;

    memset(&str, 0, sizeof(str));
    str.buf = buf;
    str.len = strlen(buf);

    res = string_encode(&str, 1, 1, 0, (unsigned char *) buf, 0, &bytes_w,
        &blk);
    ck_assert_int_eq(LMQTT_ENCODE_CONTINUE, res);
    ck_assert_int_eq(0, bytes_w);
}
END_TEST

START_TEST(should_encode_empty_string_on_zero_length_buffer)
{
    char *buf = "";
    lmqtt_string_t str;
    lmqtt_string_t *blk;
    int res;
    int bytes_w = -1;

    memset(&str, 0, sizeof(str));
    str.buf = buf;
    str.len = 0;

    res = string_encode(&str, 0, 0, 0, (unsigned char *) buf, 0, &bytes_w,
        &blk);
    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(0, bytes_w);
}
END_TEST

START_TCASE("String")
{
    ADD_TEST(should_put_chars);
    ADD_TEST(should_put_chars_with_callback);
    ADD_TEST(should_put_chars_with_blocking_decode);
    ADD_TEST(should_put_chars_with_decode_error);
    ADD_TEST(should_not_overflow_buffer);
    ADD_TEST(should_encode_non_empty_string_on_zero_length_buffer);
    ADD_TEST(should_encode_empty_string_on_zero_length_buffer);
}
END_TCASE
