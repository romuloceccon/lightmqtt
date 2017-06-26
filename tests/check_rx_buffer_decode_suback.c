#include "check_lightmqtt.h"

#define PREPARE \
    int res; \
    lmqtt_rx_buffer_t state; \
    lmqtt_subscribe_t subscribe; \
    lmqtt_subscription_t subscriptions[10]; \
    memset(&state, 0, sizeof(state)); \
    memset(&subscribe, 0, sizeof(subscribe)); \
    memset(&subscriptions, 0, sizeof(subscriptions)); \
    state.internal.value.value = &subscribe; \
    subscribe.subscriptions = subscriptions

#define DECODE_SUBACK(b, exp_cnt) \
    do { \
        size_t cnt; \
        unsigned char buf[1]; \
        lmqtt_decode_bytes_t bytes; \
        buf[0] = (b); \
        bytes.buf_len = 1; \
        bytes.buf = buf; \
        bytes.bytes_written = &cnt; \
        res = rx_buffer_decode_suback(&state, &bytes); \
        ck_assert_uint_eq((exp_cnt), cnt); \
    } while(0)

#define DECODE_SUBACK_OK(b) DECODE_SUBACK((b), 1)
#define DECODE_SUBACK_ERR(b) DECODE_SUBACK((b), 0)

START_TEST(should_decode_invalid_suback_return_code)
{
    PREPARE;

    state.internal.header.remaining_length = 3;
    state.internal.remain_buf_pos = 2;
    subscribe.count = 1;

    DECODE_SUBACK_ERR(3);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
}
END_TEST

START_TEST(should_decode_suback_with_multiple_subscriptions)
{
    PREPARE;

    state.internal.header.remaining_length = 6;
    state.internal.remain_buf_pos = 2;
    subscribe.count = 4;

    DECODE_SUBACK_OK(0);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    state.internal.remain_buf_pos++;
    DECODE_SUBACK_OK(1);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    state.internal.remain_buf_pos++;
    DECODE_SUBACK_OK(2);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    state.internal.remain_buf_pos++;
    DECODE_SUBACK_OK(0x80);
    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);

    ck_assert_int_eq(0, subscriptions[0].return_code);
    ck_assert_int_eq(1, subscriptions[1].return_code);
    ck_assert_int_eq(2, subscriptions[2].return_code);
    ck_assert_int_eq(0x80, subscriptions[3].return_code);
}
END_TEST

START_TEST(should_validate_suback_payload_len)
{
    PREPARE;

    state.internal.header.remaining_length = 5;
    state.internal.remain_buf_pos = 2;
    subscribe.count = 4;

    DECODE_SUBACK_ERR(0);
    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
}
END_TEST

START_TCASE("Rx buffer decode suback")
{
    ADD_TEST(should_decode_invalid_suback_return_code);
    ADD_TEST(should_decode_suback_with_multiple_subscriptions);
    ADD_TEST(should_validate_suback_payload_len);
}
END_TCASE
