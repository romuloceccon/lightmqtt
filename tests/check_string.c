#include "check_lightmqtt.h"

START_TEST(should_write_chars_to_buffer)
{
    char buf[3];
    lmqtt_string_t str;
    lmqtt_string_result_t res;
    size_t cnt = 0xcccc;
    int os_error = 0xcccc;

    memset(buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));

    str.buf = buf;
    str.len = sizeof(buf);

    res = string_write(&str, (unsigned char *) "ab", 2, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_SUCCESS, res);
    ck_assert_int_eq(2, cnt);
    ck_assert_int_eq(0, os_error);
    res = string_write(&str, (unsigned char *) "c", 1, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_SUCCESS, res);
    ck_assert_int_eq(1, cnt);
    ck_assert_int_eq(0, os_error);
    ck_assert_int_eq('a', buf[0]);
    ck_assert_int_eq('b', buf[1]);
    ck_assert_int_eq('c', buf[2]);
}
END_TEST

START_TEST(should_write_chars_to_nonblocking_callback)
{
    test_buffer_t buf;
    lmqtt_string_t str;
    lmqtt_string_result_t res;
    size_t cnt = 0xcccc;
    int os_error = 0xcccc;

    memset(&buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));
    str.write = &test_buffer_write;
    str.data = &buf;
    str.len = buf.len = buf.available_len = 3;

    res = string_write(&str, (unsigned char *) "ab", 2, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_SUCCESS, res);
    res = string_write(&str, (unsigned char *) "c", 1, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_SUCCESS, res);
    ck_assert_int_eq('a', (char) buf.buf[0]);
    ck_assert_int_eq('b', (char) buf.buf[1]);
    ck_assert_int_eq('c', (char) buf.buf[2]);
}
END_TEST

START_TEST(should_write_chars_to_blocking_callback)
{
    test_buffer_t buf;
    lmqtt_string_t str;
    lmqtt_string_result_t res;
    size_t cnt = 0xcccc;
    int os_error = 0xcccc;

    memset(&buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));
    str.write = &test_buffer_write;
    str.data = &buf;
    str.len = buf.len = 4;
    buf.available_len = 2;

    res = string_write(&str, (unsigned char *) "abcd", 4, &cnt, &os_error);
    ck_assert_int_eq(2, cnt);
    ck_assert_int_eq(LMQTT_STRING_SUCCESS, res);

    res = string_write(&str, (unsigned char *) "cd", 2, &cnt, &os_error);
    ck_assert_int_eq(0, cnt);
    ck_assert_int_eq(LMQTT_STRING_WOULD_BLOCK, res);

    buf.available_len = 4;
    res = string_write(&str, (unsigned char *) "cd", 2, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_SUCCESS, res);

    ck_assert_int_eq('a', (char) buf.buf[0]);
    ck_assert_int_eq('b', (char) buf.buf[1]);
    ck_assert_int_eq('c', (char) buf.buf[2]);
    ck_assert_int_eq('d', (char) buf.buf[3]);
}
END_TEST

START_TEST(should_write_chars_to_failing_callback)
{
    lmqtt_string_t str;
    lmqtt_string_result_t res;
    size_t cnt = 0xcccc;
    int os_error = 0xcccc;

    memset(&str, 0, sizeof(str));
    str.write = &test_buffer_io_fail;
    str.len = 2;

    res = string_write(&str, (unsigned char *) "a", 1, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_OS_ERROR, res);
    ck_assert_int_eq(0, cnt);
    ck_assert_int_eq(1, os_error);
}
END_TEST

START_TEST(should_not_write_chars_with_invalid_object)
{
    lmqtt_string_t str;
    lmqtt_string_result_t res;
    size_t cnt = 0xcccc;
    int os_error = 0xcccc;

    memset(&str, 0, sizeof(str));
    str.len = 2;

    res = string_write(&str, (unsigned char *) "a", 1, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_INVALID_OBJECT, res);
    ck_assert_int_eq(0, cnt);
    ck_assert_int_eq(0, os_error);
}
END_TEST

START_TEST(should_read_chars_from_buffer)
{
    char buf[3];
    lmqtt_string_t str;
    lmqtt_string_result_t res;
    size_t cnt = 0xcccc;
    int os_error = 0xcccc;

    memset(buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));

    str.buf = "abc";
    str.len = strlen(str.buf);

    res = string_read(&str, (unsigned char *) &buf[0], 2, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_SUCCESS, res);
    ck_assert_int_eq(2, cnt);
    ck_assert_int_eq(0, os_error);
    res = string_read(&str, (unsigned char *) &buf[2], 1, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_SUCCESS, res);
    ck_assert_int_eq(1, cnt);
    ck_assert_int_eq(0, os_error);
    ck_assert_int_eq('a', buf[0]);
    ck_assert_int_eq('b', buf[1]);
    ck_assert_int_eq('c', buf[2]);
}
END_TEST

START_TEST(should_read_chars_from_nonblocking_callback)
{
    char buf[3];
    test_buffer_t src;
    lmqtt_string_t str;
    lmqtt_string_result_t res;
    size_t cnt = 0xcccc;
    int os_error = 0xcccc;

    memset(&src, 0, sizeof(src));
    memset(&str, 0, sizeof(str));
    strcpy((char *) src.buf, "abc");
    str.read = &test_buffer_read;
    str.data = &src;
    str.len = src.len = src.available_len = 3;

    res = string_read(&str, (unsigned char *) &buf[0], 2, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_SUCCESS, res);
    res = string_read(&str, (unsigned char *) &buf[2], 1, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_SUCCESS, res);
    ck_assert_int_eq('a', buf[0]);
    ck_assert_int_eq('b', buf[1]);
    ck_assert_int_eq('c', buf[2]);
}
END_TEST

START_TEST(should_read_chars_from_null_buffer_with_zero_length)
{
    unsigned char buf[10];
    lmqtt_string_t str;
    lmqtt_string_result_t res;
    size_t cnt = 0xcccc;
    int os_error = 0xcccc;

    memset(&str, 0, sizeof(str));

    res = string_read(&str, buf, 0, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_SUCCESS, res);
    ck_assert_int_eq(0, cnt);
    ck_assert_int_eq(0, os_error);
}
END_TEST

START_TEST(should_read_chars_from_failing_callback_with_zero_length)
{
    unsigned char buf[10];
    lmqtt_string_t str;
    lmqtt_string_result_t res;
    size_t cnt = 0xcccc;
    int os_error = 0xcccc;

    memset(&str, 0, sizeof(str));
    str.read = &test_buffer_io_fail;

    res = string_read(&str, buf, 0, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_SUCCESS, res);
    ck_assert_int_eq(0, cnt);
    ck_assert_int_eq(0, os_error);
}
END_TEST

START_TEST(should_not_read_chars_from_string_with_both_buffer_and_callback)
{
    char buf[3];
    test_buffer_t src;
    lmqtt_string_t str;
    lmqtt_string_result_t res;
    size_t cnt = 0xcccc;
    int os_error = 0xcccc;

    memset(&src, 0, sizeof(src));
    memset(&str, 0, sizeof(str));
    str.buf = "abc";
    strcpy((char *) src.buf, str.buf);
    str.read = &test_buffer_read;
    str.data = &src;
    str.len = src.len = src.available_len = 3;

    res = string_read(&str, (unsigned char *) &buf[0], 2, &cnt, &os_error);
    ck_assert_int_eq(LMQTT_STRING_INVALID_OBJECT, res);
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

START_TEST(should_reset_start_position_before_encoding_first_char)
{
    char buf[6];
    char *src = "abcd";
    lmqtt_string_t str;
    lmqtt_encode_buffer_t enc_buf;
    int res;
    size_t bytes_w = (size_t) -1;

    memset(buf, 0, sizeof(buf));
    memset(&str, 0, sizeof(str));
    memset(&enc_buf, 0, sizeof(enc_buf));
    str.buf = src;
    str.len = strlen(src);

    res = string_encode(&str, 1, 1, 0, (unsigned char *) buf, sizeof(buf),
        &bytes_w, &enc_buf);
    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);

    res = string_encode(&str, 1, 1, 0, (unsigned char *) buf, sizeof(buf),
        &bytes_w, &enc_buf);
    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(6, bytes_w);
    ck_assert_int_eq('a', buf[2]);
    ck_assert_int_eq('d', buf[5]);
}
END_TEST

START_TCASE("String")
{
    ADD_TEST(should_write_chars_to_buffer);
    ADD_TEST(should_write_chars_to_nonblocking_callback);
    ADD_TEST(should_write_chars_to_blocking_callback);
    ADD_TEST(should_write_chars_to_failing_callback);
    ADD_TEST(should_not_write_chars_with_invalid_object);

    ADD_TEST(should_read_chars_from_buffer);
    ADD_TEST(should_read_chars_from_nonblocking_callback);
    ADD_TEST(should_read_chars_from_null_buffer_with_zero_length);
    ADD_TEST(should_read_chars_from_failing_callback_with_zero_length);
    ADD_TEST(should_not_read_chars_from_string_with_both_buffer_and_callback);

    ADD_TEST(should_encode_non_empty_string_on_zero_length_buffer);
    ADD_TEST(should_encode_empty_string_on_zero_length_buffer);
    ADD_TEST(should_encode_string_with_blocking_read);
    ADD_TEST(should_encode_string_with_read_error);
    ADD_TEST(should_reset_start_position_before_encoding_first_char);
}
END_TCASE
