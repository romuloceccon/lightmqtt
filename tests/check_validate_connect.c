#include <check.h>

#include "check_lightmqtt.h"
#include "../src/lightmqtt.c"

START_TEST(should_validate_good_connect)
{
    LMqttConnect connect;
    memset(&connect, 0, sizeof(connect));

    connect.client_id.len = 1;

    ck_assert(validate_connect(&connect));
}
END_TEST

START_TEST(should_not_validate_continued_session_without_client_id)
{
    LMqttConnect connect;
    memset(&connect, 0, sizeof(connect));

    ck_assert(!validate_connect(&connect));
}
END_TEST

START_TEST(should_validate_clean_session_without_client_id)
{
    LMqttConnect connect;
    memset(&connect, 0, sizeof(connect));

    connect.clean_session = 1;

    ck_assert(validate_connect(&connect));
}
END_TEST

START_TEST(should_validate_string_length)
{
    LMqttConnect connect;
    memset(&connect, 0, sizeof(connect));

    connect.client_id.len = -1;
    ck_assert(!validate_connect(&connect));

    connect.client_id.len = 0x10000;
    ck_assert(!validate_connect(&connect));
}
END_TEST

START_TEST(should_validate_will_topic_and_will_message)
{
    LMqttConnect connect;
    memset(&connect, 0, sizeof(connect));
    connect.client_id.len = 1;

    connect.will_topic.len = 1;
    connect.will_message.len = 0;
    ck_assert(!validate_connect(&connect));

    connect.will_topic.len = 0;
    connect.will_message.len = 1;
    ck_assert(!validate_connect(&connect));

    connect.will_topic.len = 1;
    connect.will_message.len = 1;
    ck_assert(validate_connect(&connect));
}
END_TEST

START_TEST(should_validate_will_retain_flag)
{
    LMqttConnect connect;
    memset(&connect, 0, sizeof(connect));
    connect.client_id.len = 1;

    connect.will_retain = 1;
    connect.will_topic.len = 1;
    connect.will_message.len = 1;
    ck_assert(validate_connect(&connect));

    connect.will_topic.len = 0;
    connect.will_message.len = 0;
    ck_assert(!validate_connect(&connect));
}
END_TEST

START_TEST(should_validate_user_name_and_password)
{
    LMqttConnect connect;
    memset(&connect, 0, sizeof(connect));
    connect.client_id.len = 1;

    connect.user_name.len = 1;
    connect.password.len = 1;
    ck_assert(validate_connect(&connect));

    connect.user_name.len = 0;
    connect.password.len = 1;
    ck_assert(!validate_connect(&connect));
}
END_TEST

START_TEST(should_validate_qos)
{
    LMqttConnect connect;
    memset(&connect, 0, sizeof(connect));
    connect.client_id.len = 1;

    connect.qos = 0;
    ck_assert(validate_connect(&connect));
    connect.qos = 1;
    ck_assert(validate_connect(&connect));
    connect.qos = 2;
    ck_assert(validate_connect(&connect));

    connect.qos = -1;
    ck_assert(!validate_connect(&connect));
    connect.qos = 3;
    ck_assert(!validate_connect(&connect));
}
END_TEST

TCase *tcase_validate_connect(void)
{
    TCase *result = tcase_create("Validate connect");

    tcase_add_test(result, should_validate_good_connect);
    tcase_add_test(result, should_not_validate_continued_session_without_client_id);
    tcase_add_test(result, should_validate_clean_session_without_client_id);
    tcase_add_test(result, should_validate_string_length);
    tcase_add_test(result, should_validate_will_topic_and_will_message);
    tcase_add_test(result, should_validate_will_retain_flag);
    tcase_add_test(result, should_validate_user_name_and_password);
    tcase_add_test(result, should_validate_qos);

    return result;
}
