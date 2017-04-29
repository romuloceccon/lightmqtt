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

START_TEST(should_decode_invalid_suback_return_code)
{
    PREPARE;

    state.internal.header.remaining_length = 3;
    state.internal.remain_buf_pos = 2;
    subscribe.count = 1;

    res = rx_buffer_decode_suback(&state, 3);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
}
END_TEST

START_TEST(should_decode_suback_with_multiple_subscriptions)
{
    PREPARE;

    state.internal.header.remaining_length = 6;
    state.internal.remain_buf_pos = 2;
    subscribe.count = 4;

    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, rx_buffer_decode_suback(&state, 0));
    state.internal.remain_buf_pos++;
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, rx_buffer_decode_suback(&state, 1));
    state.internal.remain_buf_pos++;
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, rx_buffer_decode_suback(&state, 2));
    state.internal.remain_buf_pos++;
    ck_assert_int_eq(LMQTT_DECODE_FINISHED, rx_buffer_decode_suback(&state, 0x80));

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

    ck_assert_int_eq(LMQTT_DECODE_ERROR, rx_buffer_decode_suback(&state, 0));
}
END_TEST

START_TCASE("Rx buffer decode suback")
{
    ADD_TEST(should_decode_invalid_suback_return_code);
    ADD_TEST(should_decode_suback_with_multiple_subscriptions);
    ADD_TEST(should_validate_suback_payload_len);
}
END_TCASE
