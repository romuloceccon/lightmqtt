#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

static int test_lookup_packet_result;
static int test_packet_id;
static lmqtt_operation_t test_operation;
static void *test_packet;

int lookup_packet(int packet_id, lmqtt_operation_t operation, void **packet)
{
    test_packet_id = packet_id;
    test_operation = operation;

    *packet = test_packet;
    return test_lookup_packet_result;
}

START_TEST(should_decode_suback_packet_id)
{
    lmqtt_subscription_t subscription;
    lmqtt_subscribe_t subscribe;
    lmqtt_suback_t suback;
    memset(&subscription, 0, sizeof(subscription));
    memset(&subscribe, 0, sizeof(subscribe));
    memset(&suback, 0, sizeof(suback));
    subscribe.count = 1;
    subscribe.subscriptions = &subscription;
    test_lookup_packet_result = 1;
    test_packet = &subscribe;

    ck_assert_int_eq(LMQTT_DECODE_CONTINUE,
        suback_decode(&suback, 3, lookup_packet, 0x0a));
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE,
        suback_decode(&suback, 3, lookup_packet, 0x0b));

    ck_assert_uint_eq(0x0a0b, suback.packet_id);
}
END_TEST

START_TEST(should_decode_suback_with_invalid_payload_len)
{
    lmqtt_subscribe_t subscribe;
    lmqtt_suback_t suback;
    memset(&suback, 0, sizeof(suback));
    test_lookup_packet_result = 1;
    test_packet = &subscribe;

    ck_assert_int_eq(LMQTT_DECODE_ERROR,
        suback_decode(&suback, 2, lookup_packet, 0x0a));
    ck_assert_int_eq(1, suback.internal.failed);
}
END_TEST

START_TEST(should_decode_suback_with_nonexistent_packet_id)
{
    lmqtt_suback_t suback;
    memset(&suback, 0, sizeof(suback));
    test_lookup_packet_result = 0;
    test_packet = 0;

    ck_assert_int_eq(LMQTT_DECODE_CONTINUE,
        suback_decode(&suback, 3, lookup_packet, 0x0a));
    ck_assert_int_eq(LMQTT_DECODE_ERROR,
        suback_decode(&suback, 3, lookup_packet, 0x0b));
    ck_assert_int_eq(0x0a0b, test_packet_id);
    ck_assert_int_eq(LMQTT_OPERATION_SUBSCRIBE, test_operation);
}
END_TEST

START_TEST(should_decode_suback_with_different_subscription_len)
{
    lmqtt_subscription_t subscription;
    lmqtt_subscribe_t subscribe;
    lmqtt_suback_t suback;
    memset(&subscription, 0, sizeof(subscription));
    memset(&subscribe, 0, sizeof(subscribe));
    memset(&suback, 0, sizeof(suback));
    subscribe.count = 1;
    subscribe.subscriptions = &subscription;
    test_lookup_packet_result = 1;
    test_packet = &subscribe;

    ck_assert_int_eq(LMQTT_DECODE_CONTINUE,
        suback_decode(&suback, 4, lookup_packet, 0x0a));
    ck_assert_int_eq(LMQTT_DECODE_ERROR,
        suback_decode(&suback, 4, lookup_packet, 0x0b));
    ck_assert_int_eq(0x0a0b, test_packet_id);
    ck_assert_int_eq(LMQTT_OPERATION_SUBSCRIBE, test_operation);
}
END_TEST

START_TEST(should_decode_suback_return_codes)
{
    lmqtt_subscription_t subscriptions[2];
    lmqtt_subscribe_t subscribe;
    lmqtt_suback_t suback;
    memset(&subscriptions, 0, sizeof(subscriptions));
    memset(&subscribe, 0, sizeof(subscribe));
    memset(&suback, 0, sizeof(suback));
    subscribe.count = 2;
    subscribe.subscriptions = subscriptions;
    test_lookup_packet_result = 1;
    test_packet = &subscribe;

    ck_assert_int_eq(LMQTT_DECODE_CONTINUE,
        suback_decode(&suback, 4, lookup_packet, 0x0a));
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE,
        suback_decode(&suback, 4, lookup_packet, 0x0b));
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE,
        suback_decode(&suback, 4, lookup_packet, 1));
    ck_assert_int_eq(LMQTT_DECODE_FINISHED,
        suback_decode(&suback, 4, lookup_packet, 2));

    ck_assert_int_eq(1, subscriptions[0].return_code);
    ck_assert_int_eq(2, subscriptions[1].return_code);
}
END_TEST

START_TEST(should_decode_invalid_suback_return_code)
{
    lmqtt_subscription_t subscription;
    lmqtt_subscribe_t subscribe;
    lmqtt_suback_t suback;
    memset(&subscription, 0, sizeof(subscription));
    memset(&subscribe, 0, sizeof(subscribe));
    memset(&suback, 0, sizeof(suback));
    subscribe.count = 1;
    subscribe.subscriptions = &subscription;
    test_lookup_packet_result = 1;
    test_packet = &subscribe;

    ck_assert_int_eq(LMQTT_DECODE_CONTINUE,
        suback_decode(&suback, 3, lookup_packet, 0x0a));
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE,
        suback_decode(&suback, 3, lookup_packet, 0x0b));
    ck_assert_int_eq(LMQTT_DECODE_ERROR,
        suback_decode(&suback, 3, lookup_packet, 3));
}
END_TEST

START_TCASE("SUBACK decode")
{
    ADD_TEST(should_decode_suback_packet_id);
    ADD_TEST(should_decode_suback_with_invalid_payload_len);
    ADD_TEST(should_decode_suback_with_nonexistent_packet_id);
    ADD_TEST(should_decode_suback_with_different_subscription_len);
    ADD_TEST(should_decode_suback_return_codes);
    ADD_TEST(should_decode_invalid_suback_return_code);
}
END_TCASE
