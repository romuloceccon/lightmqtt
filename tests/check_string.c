#include "check_lightmqtt.h"

typedef struct {
    u8 buf[10];
    int len;
    int pos;
} test_write_buffer_t;

lmqtt_write_result_t test_write(void *data, u8 *buf, int len, int *bytes_w)
{
    test_write_buffer_t *buffer = data;
    int i;

    *bytes_w = 0;

    for (i = 0; i < len; i++) {
        if (buffer->pos >= buffer->len)
            return LMQTT_WRITE_WOULD_BLOCK;
        buffer->buf[buffer->pos++] = buf[i];
        *bytes_w += 1;
    }

    return LMQTT_WRITE_SUCCESS;
}

lmqtt_write_result_t test_write_fail(void *data, u8 *buf, int len, int *bytes_w)
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

START_TCASE("String")
{
    ADD_TEST(should_put_chars);
    ADD_TEST(should_put_chars_with_callback);
    ADD_TEST(should_put_chars_with_blocking_decode);
    ADD_TEST(should_put_chars_with_decode_error);
    ADD_TEST(should_not_overflow_buffer);
}
END_TCASE
