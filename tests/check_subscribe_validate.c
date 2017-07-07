#include "check_lightmqtt.h"

#define PREPARE \
    lmqtt_subscribe_t subscribe; \
    lmqtt_subscription_t subscriptions[2]; \
    memset(&subscribe, 0, sizeof(subscribe)); \
    memset(&subscriptions, 0, sizeof(subscriptions))

START_TEST(should_validate_good_subscribe)
{
    PREPARE;

    subscribe.subscriptions = &subscriptions[0];
    subscribe.count = 1;
    subscriptions[0].requested_qos = LMQTT_QOS_2;
    subscriptions[0].topic.buf = "test";
    subscriptions[0].topic.len = strlen(subscriptions[0].topic.buf);

    ck_assert_int_eq(1, lmqtt_subscribe_validate(&subscribe));
}
END_TEST

START_TEST(should_validate_with_zero_subscriptions)
{
    PREPARE;

    ck_assert_int_eq(0, lmqtt_subscribe_validate(&subscribe));
}
END_TEST

START_TEST(should_validate_with_null_subscriptions)
{
    PREPARE;

    subscribe.count = 1;

    ck_assert_int_eq(0, lmqtt_subscribe_validate(&subscribe));
}
END_TEST

START_TEST(should_validate_with_empty_topic)
{
    PREPARE;

    subscribe.subscriptions = &subscriptions[0];
    subscribe.count = 1;

    ck_assert_int_eq(0, lmqtt_subscribe_validate(&subscribe));
}
END_TEST

START_TEST(should_validate_with_invalid_requested_qos)
{
    PREPARE;

    subscribe.subscriptions = &subscriptions[0];
    subscribe.count = 1;
    subscriptions[0].topic.buf = "test";
    subscriptions[0].topic.len = strlen(subscriptions[0].topic.buf);
    subscriptions[0].requested_qos = 3;

    ck_assert_int_eq(0, lmqtt_subscribe_validate(&subscribe));
}
END_TEST

START_TEST(should_validate_multiple_topics)
{
    PREPARE;

    subscribe.subscriptions = &subscriptions[0];
    subscribe.count = 2;
    subscriptions[0].requested_qos = LMQTT_QOS_2;
    subscriptions[0].topic.buf = "a";
    subscriptions[0].topic.len = strlen(subscriptions[0].topic.buf);
    subscriptions[1].requested_qos = -1;
    subscriptions[1].topic.buf = "b";
    subscriptions[1].topic.len = strlen(subscriptions[1].topic.buf);

    ck_assert_int_eq(0, lmqtt_subscribe_validate(&subscribe));
}
END_TEST

START_TEST(should_validate_with_too_long_string)
{
    PREPARE;

    subscribe.subscriptions = &subscriptions[0];
    subscribe.count = 1;
    subscriptions[0].requested_qos = LMQTT_QOS_0;
    subscriptions[0].topic.buf = "x";
    subscriptions[0].topic.len = 0x10000;

    ck_assert_int_eq(0, lmqtt_subscribe_validate(&subscribe));
}
END_TEST

START_TCASE("Subscribe validate")
{
    ADD_TEST(should_validate_good_subscribe);
    ADD_TEST(should_validate_with_zero_subscriptions);
    ADD_TEST(should_validate_with_null_subscriptions);
    ADD_TEST(should_validate_with_empty_topic);
    ADD_TEST(should_validate_with_invalid_requested_qos);
    ADD_TEST(should_validate_multiple_topics);
    ADD_TEST(should_validate_with_too_long_string);
}
END_TCASE
