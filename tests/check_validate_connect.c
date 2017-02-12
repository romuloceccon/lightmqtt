#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

START_TEST(should_validate_good_connect)
{
    lmqtt_connect_t connect;
    memset(&connect, 0, sizeof(connect));

    connect.client_id.len = 1;

    ck_assert(validate_connect(&connect));
}
END_TEST

START_TEST(should_not_validate_continued_session_without_client_id)
{
    lmqtt_connect_t connect;
    memset(&connect, 0, sizeof(connect));

    ck_assert(!validate_connect(&connect));
}
END_TEST

START_TEST(should_validate_clean_session_without_client_id)
{
    lmqtt_connect_t connect;
    memset(&connect, 0, sizeof(connect));

    connect.clean_session = 1;

    ck_assert(validate_connect(&connect));
}
END_TEST

START_TEST(should_validate_string_length)
{
    lmqtt_connect_t connect;
    memset(&connect, 0, sizeof(connect));

    connect.client_id.len = -1;
    ck_assert(!validate_connect(&connect));

    connect.client_id.len = 0x10000;
    ck_assert(!validate_connect(&connect));
}
END_TEST

START_TEST(should_validate_will_topic_and_will_message)
{
    lmqtt_connect_t connect;
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
    lmqtt_connect_t connect;
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
    lmqtt_connect_t connect;
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
    lmqtt_connect_t connect;
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

START_TCASE("Validate connect")
{
    ADD_TEST(should_validate_good_connect);
    ADD_TEST(should_not_validate_continued_session_without_client_id);
    ADD_TEST(should_validate_clean_session_without_client_id);
    ADD_TEST(should_validate_string_length);
    ADD_TEST(should_validate_will_topic_and_will_message);
    ADD_TEST(should_validate_will_retain_flag);
    ADD_TEST(should_validate_user_name_and_password);
    ADD_TEST(should_validate_qos);
}
END_TCASE
