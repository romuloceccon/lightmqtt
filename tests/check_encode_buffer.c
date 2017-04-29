#include "check_lightmqtt.h"

#define PREPARE \
    u8 buf[512]; \
    int bytes_w = 0xcccccccc; \
    lmqtt_encode_buffer_t encode_buffer; \
    lmqtt_encode_result_t res; \
    int cnt; \
    lmqtt_store_value_t value; \
    memset(buf, 0, sizeof(buf)); \
    memset(&encode_buffer, 0, sizeof(encode_buffer)); \
    memset(&value, 0, sizeof(value)); \
    value.value = &cnt

static lmqtt_encode_result_t test_builder(lmqtt_store_value_t *value,
    lmqtt_encode_buffer_t *encode_buffer)
{
    int i;
    int cnt = *(int *) value->value;

    for (i = 0; i < cnt; i++)
        encode_buffer->buf[i] = i + 1;
    encode_buffer->buf_len = cnt;

    return LMQTT_ENCODE_FINISHED;
}

static lmqtt_encode_result_t test_builder_failure(lmqtt_store_value_t *value,
    lmqtt_encode_buffer_t *encode_buffer)
{
    return LMQTT_ENCODE_ERROR;
}

START_TEST(should_encode_with_sufficient_buffer)
{
    PREPARE;

    cnt = 10;
    res = encode_buffer_encode(&encode_buffer, &value, test_builder, 0, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(10, bytes_w);
    ck_assert_int_eq(0, encode_buffer.buf_len);
    ck_assert_int_eq(0, encode_buffer.encoded);
    ck_assert_uint_eq(1, buf[0]);
    ck_assert_uint_eq(10, buf[9]);
    ck_assert_uint_eq(0, buf[10]);
}
END_TEST

START_TEST(should_encode_from_offset)
{
    PREPARE;

    cnt = 10;
    res = encode_buffer_encode(&encode_buffer, &value, test_builder, 5, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(5, bytes_w);
    ck_assert_int_eq(0, encode_buffer.buf_len);
    ck_assert_int_eq(0, encode_buffer.encoded);
    ck_assert_uint_eq(6, buf[0]);
    ck_assert_uint_eq(10, buf[4]);
    ck_assert_uint_eq(0, buf[5]);
}
END_TEST

START_TEST(should_encode_with_insufficient_buffer)
{
    PREPARE;

    cnt = 10;
    res = encode_buffer_encode(&encode_buffer, &value, test_builder, 0, buf,
        5, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_CONTINUE, res);
    ck_assert_int_eq(5, bytes_w);
    ck_assert_int_eq(10, encode_buffer.buf_len);
    ck_assert_int_eq(1, encode_buffer.encoded);
    ck_assert_uint_eq(1, buf[0]);
    ck_assert_uint_eq(5, buf[4]);
    ck_assert_uint_eq(0, buf[5]);
}
END_TEST

START_TEST(should_encode_at_zero_length_buffer)
{
    PREPARE;

    cnt = 10;
    res = encode_buffer_encode(&encode_buffer, &value, test_builder, 0, buf,
        0, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_CONTINUE, res);
    ck_assert_int_eq(0, bytes_w);
    ck_assert_int_eq(10, encode_buffer.buf_len);
    ck_assert_int_eq(1, encode_buffer.encoded);
}
END_TEST

START_TEST(should_not_build_twice)
{
    PREPARE;

    cnt = 10;
    res = encode_buffer_encode(&encode_buffer, &value, test_builder, 0, buf,
        5, &bytes_w);

    encode_buffer.buf[5] = 0xcc;
    encode_buffer.buf[9] = 0xcc;

    res = encode_buffer_encode(&encode_buffer, &value, test_builder, 5, buf,
        sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(5, bytes_w);
    ck_assert_int_eq(0, encode_buffer.buf_len);
    ck_assert_int_eq(0, encode_buffer.encoded);
    ck_assert_uint_eq(0xcc, buf[0]);
    ck_assert_uint_eq(0xcc, buf[4]);
    ck_assert_uint_eq(0, buf[5]);
}
END_TEST

START_TEST(should_handle_build_failure)
{
    PREPARE;

    res = encode_buffer_encode(&encode_buffer, &value, test_builder_failure, 0,
        buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_ERROR, res);
    ck_assert_int_eq(0, bytes_w);
}
END_TEST

START_TCASE("Encode buffer")
{
    ADD_TEST(should_encode_with_sufficient_buffer);
    ADD_TEST(should_encode_from_offset);
    ADD_TEST(should_encode_with_insufficient_buffer);
    ADD_TEST(should_encode_at_zero_length_buffer);
    ADD_TEST(should_not_build_twice);
    ADD_TEST(should_handle_build_failure);
}
END_TCASE
