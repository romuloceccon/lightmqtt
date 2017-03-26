#include "check_lightmqtt.h"

START_TEST(should_validate_good_publish)
{
    lmqtt_publish_t publish;

    memset(&publish, 0, sizeof(publish));
    publish.topic.buf = "topic";
    publish.topic.len = strlen(publish.topic.buf);

    ck_assert_int_eq(1, lmqtt_publish_validate(&publish));
}
END_TEST

START_TEST(should_validate_publish_with_maximum_payload)
{
    lmqtt_publish_t publish;

    memset(&publish, 0, sizeof(publish));
    publish.topic.buf = "topic";
    publish.topic.len = strlen(publish.topic.buf);
    publish.payload.len = 268435455 - 9;

    ck_assert_int_eq(1, lmqtt_publish_validate(&publish));
}
END_TEST

START_TEST(should_validate_publish_with_too_big_payload)
{
    lmqtt_publish_t publish;

    memset(&publish, 0, sizeof(publish));
    publish.topic.buf = "topic";
    publish.topic.len = strlen(publish.topic.buf);
    publish.payload.len = 268435455 - 9 + 1;

    ck_assert_int_eq(0, lmqtt_publish_validate(&publish));
}
END_TEST

START_TEST(should_validate_publish_with_empty_topic)
{
    lmqtt_publish_t publish;

    memset(&publish, 0, sizeof(publish));

    ck_assert_int_eq(0, lmqtt_publish_validate(&publish));
}
END_TEST

START_TEST(should_validate_publish_with_bad_qos)
{
    lmqtt_publish_t publish;

    memset(&publish, 0, sizeof(publish));
    publish.topic.buf = "topic";
    publish.topic.len = strlen(publish.topic.buf);
    publish.qos = 3;

    ck_assert_int_eq(0, lmqtt_publish_validate(&publish));
}
END_TEST

START_TCASE("Publish validate")
{
    ADD_TEST(should_validate_good_publish);
    ADD_TEST(should_validate_publish_with_maximum_payload);
    ADD_TEST(should_validate_publish_with_too_big_payload);
    ADD_TEST(should_validate_publish_with_empty_topic);
    ADD_TEST(should_validate_publish_with_bad_qos);
}
END_TCASE
