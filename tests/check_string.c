#include "check_lightmqtt.h"

typedef struct {
    unsigned char buf[10];
    size_t len;
    size_t pos;
} test_write_buffer_t;

lmqtt_io_result_t test_write(void *data, void *buf, size_t len, size_t *bytes_w,
    int *os_error)
{
    test_write_buffer_t *buffer = data;
    unsigned char *buf_c = buf;
    int i;

    *bytes_w = 0;

    for (i = 0; i < len; i++) {
        if (buffer->pos >= buffer->len)
            break;
        buffer->buf[buffer->pos++] = buf_c[i];
        *bytes_w += 1;
    }

    return *bytes_w > 0 || len == 0 ? LMQTT_IO_SUCCESS : LMQTT_IO_WOULD_BLOCK;
}

START_TEST(should_put_chars)
{
    char buf[3];
    lmqtt_string_t str;
    lmqtt_string_t *blk;
    int res;
    size_t cnt;

    memset(buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));

    str.buf = buf;
    str.len = sizeof(buf);

    res = string_put(&str, (unsigned char *) "ab", 2, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    ck_assert_int_eq(2, cnt);
    res = string_put(&str, (unsigned char *) "c", 1, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(1, cnt);
    ck_assert_int_eq('a', buf[0]);
    ck_assert_int_eq('b', buf[1]);
    ck_assert_int_eq('c', buf[2]);
}
END_TEST

START_TEST(should_put_chars_with_callback)
{
    test_write_buffer_t buf;
    lmqtt_string_t str;
    lmqtt_string_t *blk;
    int res;
    size_t cnt;

    memset(&buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));
    str.write = &test_write;
    str.data = &buf;
    str.len = 3;
    buf.len = 3;

    res = string_put(&str, (unsigned char *) "ab", 2, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    res = string_put(&str, (unsigned char *) "c", 1, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq('a', buf.buf[0]);
    ck_assert_int_eq('b', buf.buf[1]);
    ck_assert_int_eq('c', buf.buf[2]);
}
END_TEST

START_TEST(should_put_chars_with_blocking_decode)
{
    test_write_buffer_t buf;
    lmqtt_string_t str;
    lmqtt_string_t *blk = &str;
    int res;
    size_t cnt;

    memset(&buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));
    str.write = &test_write;
    str.data = &buf;
    str.len = 2;
    buf.len = 1;

    res = string_put(&str, (unsigned char *) "a", 1, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    ck_assert_ptr_eq(NULL, blk);
    res = string_put(&str, (unsigned char *) "b", 1, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_WOULD_BLOCK, res);
    ck_assert_ptr_eq(&str, blk);
}
END_TEST

START_TEST(should_put_remaining_chars_after_blocking_decode)
{
    test_write_buffer_t buf;
    lmqtt_string_t str;
    lmqtt_string_t *blk;
    int res;
    size_t cnt;

    memset(&buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));
    str.write = &test_write;
    str.data = &buf;
    str.len = 3;
    buf.len = 1;

    res = string_put(&str, (unsigned char *) "ab", 2, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    ck_assert_int_eq(1, cnt);

    buf.len = 3;

    res = string_put(&str, (unsigned char *) "bc", 2, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(2, cnt);
    ck_assert_int_eq('a', buf.buf[0]);
    ck_assert_int_eq('b', buf.buf[1]);
    ck_assert_int_eq('c', buf.buf[2]);
}
END_TEST

START_TEST(should_put_single_char_after_blocking_decode)
{
    test_write_buffer_t buf;
    lmqtt_string_t str;
    lmqtt_string_t *blk;
    int res;
    size_t cnt;

    memset(&buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));
    str.write = &test_write;
    str.data = &buf;
    str.len = 3;
    buf.len = 1;

    res = string_put(&str, (unsigned char *) "abc", 3, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    ck_assert_int_eq(1, cnt);

    buf.len = 3;

    res = string_put(&str, (unsigned char *) "b", 1, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    ck_assert_int_eq(1, cnt);
}
END_TEST

START_TEST(should_put_chars_with_decode_error)
{
    lmqtt_string_t str;
    lmqtt_string_t *blk;
    int res;
    size_t cnt;

    memset(&str, 0, sizeof(str));
    str.write = &test_buffer_io_fail;
    str.len = 2;

    res = string_put(&str, (unsigned char *) "a", 1, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
}
END_TEST

START_TEST(should_not_overflow_buffer_with_single_char)
{
    char buf[2];
    lmqtt_string_t str;
    lmqtt_string_t *blk;
    int res;
    size_t cnt;

    memset(buf, 0x7c, sizeof(buf));
    memset(&str, 0, sizeof(str));

    str.buf = buf;
    str.len = 1;

    res = string_put(&str, (unsigned char *) "a", 1, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    res = string_put(&str, (unsigned char *) "b", 1, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(0, cnt);
    ck_assert_int_eq('a', buf[0]);
    ck_assert_int_eq(0x7c, buf[1]);
}
END_TEST

START_TEST(should_not_overflow_buffer_with_multiple_chars)
{
    char buf[2];
    lmqtt_string_t str;
    lmqtt_string_t *blk;
    int res;
    size_t cnt = 0xcccccccc;

    memset(buf, 0x7c, sizeof(buf));
    memset(&str, 0, sizeof(str));

    str.buf = buf;
    str.len = 1;

    res = string_put(&str, (unsigned char *) "ab", 2, &cnt, &blk);
    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(0, cnt);
    ck_assert_int_eq(0x7c, buf[0]);
}
END_TEST

START_TEST(should_encode_non_empty_string_on_zero_length_buffer)
{
    char *buf = "test";
    lmqtt_string_t str;
    lmqtt_encode_buffer_t enc_buf;
    int res;
    size_t bytes_w = (size_t) -1;

    memset(&str, 0, sizeof(str));
    memset(&enc_buf, 0, sizeof(enc_buf));
    str.buf = buf;
    str.len = strlen(buf);

    res = string_encode(&str, 1, 1, 0, (unsigned char *) buf, 0, &bytes_w,
        &enc_buf);
    ck_assert_int_eq(LMQTT_ENCODE_CONTINUE, res);
    ck_assert_int_eq(0, bytes_w);
}
END_TEST

START_TEST(should_encode_empty_string_on_zero_length_buffer)
{
    char *buf = "";
    lmqtt_string_t str;
    lmqtt_encode_buffer_t enc_buf;
    int res;
    size_t bytes_w = (size_t) -1;

    memset(&str, 0, sizeof(str));
    memset(&enc_buf, 0, sizeof(enc_buf));
    str.buf = buf;
    str.len = 0;

    res = string_encode(&str, 0, 0, 0, (unsigned char *) buf, 0, &bytes_w,
        &enc_buf);
    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(0, bytes_w);
}
END_TEST

START_TEST(should_encode_string_with_blocking_read)
{
    unsigned char buf[64];
    test_buffer_t test_buf;
    lmqtt_string_t str;
    lmqtt_encode_buffer_t enc_buf;
    int res;
    size_t bytes_w = (size_t) -1;

    memset(&test_buf, 0, sizeof(test_buf));
    memset(&str, 0, sizeof(str));
    memset(&enc_buf, 0, sizeof(enc_buf));
    str.data = &test_buf;
    str.read = &test_buffer_read;
    str.len = 10;
    test_buf.len = str.len;

    res = string_encode(&str, 0, 0, 0, buf, sizeof(buf), &bytes_w, &enc_buf);
    ck_assert_int_eq(LMQTT_ENCODE_WOULD_BLOCK, res);
    ck_assert_int_eq(0, bytes_w);
    ck_assert_ptr_eq(&str, enc_buf.blocking_str);
}
END_TEST

START_TEST(should_encode_string_with_read_error)
{
    unsigned char buf[64];
    lmqtt_string_t str;
    lmqtt_encode_buffer_t enc_buf;
    int res;
    size_t bytes_w = (size_t) -1;

    memset(&str, 0, sizeof(str));
    memset(&enc_buf, 0, sizeof(enc_buf));
    str.read = &test_buffer_io_fail;
    str.len = 10;

    res = string_encode(&str, 0, 0, 0, buf, sizeof(buf), &bytes_w, &enc_buf);
    ck_assert_int_eq(LMQTT_ENCODE_ERROR, res);
    ck_assert_int_eq(LMQTT_ERROR_ENCODE_STRING, enc_buf.error);
    ck_assert_int_eq(1, enc_buf.os_error);
}
END_TEST

START_TCASE("String")
{
    ADD_TEST(should_put_chars);
    ADD_TEST(should_put_chars_with_callback);
    ADD_TEST(should_put_chars_with_blocking_decode);
    ADD_TEST(should_put_remaining_chars_after_blocking_decode);
    ADD_TEST(should_put_single_char_after_blocking_decode);
    ADD_TEST(should_put_chars_with_decode_error);
    ADD_TEST(should_not_overflow_buffer_with_single_char);
    ADD_TEST(should_not_overflow_buffer_with_multiple_chars);
    ADD_TEST(should_encode_non_empty_string_on_zero_length_buffer);
    ADD_TEST(should_encode_empty_string_on_zero_length_buffer);
    ADD_TEST(should_encode_string_with_blocking_read);
    ADD_TEST(should_encode_string_with_read_error);
}
END_TCASE
