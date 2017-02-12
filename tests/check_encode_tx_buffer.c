#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

#define BYTES_W_PLACEHOLDER -12345
#define BUF_PLACEHOLDER 0xcc

#define PREPARE \
    int res; \
    int data = 0; \
    int bytes_w = BYTES_W_PLACEHOLDER; \
    u8 buf[256]; \
    lmqtt_encode_t recipe[10]; \
    lmqtt_tx_buffer_state_t state; \
    memset(recipe, 0, sizeof(recipe)); \
    memset(&state, 0, sizeof(state)); \
    state.recipe = recipe; \
    state.data = &data; \
    memset(buf, BUF_PLACEHOLDER, sizeof(buf))

/*
 * To easily test the tx buffer building we use very simple encoding methods
 * named `encode_test_<begin>_<end>`, meaning it fills the given buffer with the
 * the value pointed to by `data`, adding the values from <begin> to <end>. For
 * example, given data=100 and offset=0 the method `encode_test_0_9` fills the
 * buffer with values (100, 101, 102, ... 108, 109). Given data=10 and offset=8
 * the buffer is filled with (18, 19).
 */

static int encode_test_range(int *data, int offset, u8 *buf, int buf_len,
     int *bytes_written, int begin, int end)
{
    int i;
    int pos = 0;

    assert(buf_len >= 0);

    *bytes_written = 0;
    for (i = begin + offset; i <= end; i++) {
        if (pos >= buf_len)
            return LMQTT_ENCODE_AGAIN;
        buf[pos++] = *data + i;
        *bytes_written += 1;
    }
    return LMQTT_ENCODE_FINISHED;
}

static int encode_test_0_9(int *data, int offset, u8 *buf, int buf_len,
    int *bytes_written)
{
    return encode_test_range(data, offset, buf, buf_len, bytes_written, 0, 9);
}

static int encode_test_50_54(int *data, int offset, u8 *buf, int buf_len,
    int *bytes_written)
{
    return encode_test_range(data, offset, buf, buf_len, bytes_written, 50, 54);
}

static int encode_test_fail(int *data, int offset, u8 *buf, int buf_len,
    int *bytes_written)
{
    return LMQTT_ENCODE_ERROR;
}

START_TEST(should_encode_tx_buffer_with_one_encoding_function)
{
    PREPARE;

    recipe[0] = (lmqtt_encode_t) encode_test_0_9;

    res = encode_tx_buffer(&state, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(10, bytes_w);

    ck_assert_uint_eq(0, buf[0]);
    ck_assert_uint_eq(9, buf[9]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[10]);
}
END_TEST

START_TEST(should_encode_tx_buffer_with_two_encoding_functions)
{
    PREPARE;

    recipe[0] = (lmqtt_encode_t) encode_test_0_9;
    recipe[1] = (lmqtt_encode_t) encode_test_50_54;

    res = encode_tx_buffer(&state, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(15, bytes_w);

    ck_assert_uint_eq(0, buf[0]);
    ck_assert_uint_eq(9, buf[9]);
    ck_assert_uint_eq(50, buf[10]);
    ck_assert_uint_eq(54, buf[14]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[15]);
}
END_TEST

START_TEST(should_stop_recipe_after_buffer_fills_up)
{
    PREPARE;

    recipe[0] = (lmqtt_encode_t) encode_test_0_9;
    recipe[1] = (lmqtt_encode_t) encode_test_50_54;

    res = encode_tx_buffer(&state, buf, 5, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_AGAIN, res);
    ck_assert_int_eq(5, bytes_w);

    ck_assert_uint_eq(0, buf[0]);
    ck_assert_uint_eq(4, buf[4]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[5]);
}
END_TEST

START_TEST(should_return_actual_bytes_written_after_error)
{
    PREPARE;

    recipe[0] = (lmqtt_encode_t) encode_test_0_9;
    recipe[1] = (lmqtt_encode_t) encode_test_50_54;
    recipe[2] = (lmqtt_encode_t) encode_test_fail;

    res = encode_tx_buffer(&state, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_ERROR, res);
    ck_assert_int_eq(15, bytes_w);

    ck_assert_uint_eq(0, buf[0]);
    ck_assert_uint_eq(54, buf[14]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[15]);
}
END_TEST

START_TEST(should_continue_buffer_where_previous_call_stopped)
{
    PREPARE;

    recipe[0] = (lmqtt_encode_t) encode_test_0_9;
    recipe[1] = (lmqtt_encode_t) encode_test_50_54;

    res = encode_tx_buffer(&state, buf, 6, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_AGAIN, res);
    ck_assert_int_eq(6, bytes_w);

    ck_assert_uint_eq(0, buf[0]);
    ck_assert_uint_eq(5, buf[5]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[6]);

    res = encode_tx_buffer(&state, buf, 6, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_AGAIN, res);
    ck_assert_int_eq(6, bytes_w);

    ck_assert_uint_eq(6, buf[0]);
    ck_assert_uint_eq(51, buf[5]);
    ck_assert_uint_eq(BUF_PLACEHOLDER, buf[6]);

    res = encode_tx_buffer(&state, buf, 6, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);
    ck_assert_int_eq(3, bytes_w);

    ck_assert_uint_eq(52, buf[0]);
    ck_assert_uint_eq(54, buf[2]);
}
END_TEST

START_TEST(should_continue_buffer_twice_with_the_same_recipe_entry)
{
    PREPARE;

    recipe[0] = (lmqtt_encode_t) encode_test_0_9;

    res = encode_tx_buffer(&state, buf, 2, &bytes_w);

    res = encode_tx_buffer(&state, buf, 2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_AGAIN, res);
    ck_assert_uint_eq(2, buf[0]);
    ck_assert_uint_eq(3, buf[1]);

    res = encode_tx_buffer(&state, buf, 2, &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_AGAIN, res);
    ck_assert_uint_eq(4, buf[0]);
    ck_assert_uint_eq(5, buf[1]);
}
END_TEST

START_TCASE("Build tx buffer")
{
    ADD_TEST(should_encode_tx_buffer_with_one_encoding_function);
    ADD_TEST(should_encode_tx_buffer_with_two_encoding_functions);
    ADD_TEST(should_stop_recipe_after_buffer_fills_up);
    ADD_TEST(should_return_actual_bytes_written_after_error);
    ADD_TEST(should_continue_buffer_where_previous_call_stopped);
    ADD_TEST(should_continue_buffer_twice_with_the_same_recipe_entry);
}
END_TCASE
