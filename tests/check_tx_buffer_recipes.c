#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

#define PREPARE \
    u8 buf[512]; \
    lmqtt_tx_buffer_t state; \
    lmqtt_io_result_t res; \
    int bytes_written; \
    memset(buf, 0xcc, sizeof(buf)); \
    memset(&state, 0xcc, sizeof(state))

START_TEST(should_encode_connect)
{
    u8 *expected_buffer;
    int i;
    lmqtt_connect_t connect;

    PREPARE;
    memset(&connect, 0, sizeof(connect));

    connect.keep_alive = 0x102;
    connect.client_id.buf = "a";
    connect.client_id.len = 1;
    connect.will_topic.buf = "b";
    connect.will_topic.len = 1;
    connect.will_message.buf = "c";
    connect.will_message.len = 1;
    connect.user_name.buf = "d";
    connect.user_name.len = 1;
    connect.password.buf = "e";
    connect.password.len = 1;

    lmqtt_tx_buffer_connect(&state, &connect);

    ck_assert(&tx_buffer_finder_connect == state.finder);
    ck_assert_ptr_eq(&connect, state.data);

    ck_assert_int_eq(0, state.internal.pos);
    ck_assert_int_eq(0, state.internal.offset);

    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(27, bytes_written);

    expected_buffer = (u8 *) "\x10\x19\x00\x04" "MQTT\x04\xc4\x01\x02\x00\x01"
        "a\x00\x01" "b\x00\x01" "c\x00\x01" "d\x00\x01" "e";

    for (i = 0; i < 27; i++)
        ck_assert_int_eq(expected_buffer[i], buf[i]);
}
END_TEST

START_TEST(should_encode_subscribe_to_one_topic)
{
    u8 *expected_buffer;
    int i;
    lmqtt_subscribe_t subscribe;
    lmqtt_subscription_t subscription;

    PREPARE;
    memset(&subscribe, 0, sizeof(subscribe));
    memset(&subscription, 0, sizeof(subscription));

    subscribe.packet_id = 0x0a0b;
    subscribe.count = 1;
    subscribe.subscriptions = &subscription;

    subscription.qos = 2;
    subscription.topic.buf = "test";
    subscription.topic.len = 4;

    lmqtt_tx_buffer_subscribe(&state, &subscribe);

    ck_assert(&tx_buffer_finder_subscribe == state.finder);
    ck_assert_ptr_eq(&subscribe, state.data);

    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(11, bytes_written);

    expected_buffer = (u8 *) "\x82\x09\x0a\x0b\x00\x04" "test\x02";
    for (i = 0; i < 11; i++)
        ck_assert_int_eq(expected_buffer[i], buf[i]);
}
END_TEST

START_TEST(should_encode_subscribe_to_multiple_topics)
{
    lmqtt_subscribe_t subscribe;
    lmqtt_subscription_t subscriptions[2];
    char topic_1[127];
    char topic_2[129];

    PREPARE;
    memset(&subscribe, 0, sizeof(subscribe));
    memset(&subscriptions, 0, sizeof(subscriptions));
    memset(topic_1, 'x', sizeof(topic_1));
    memset(topic_2, 'y', sizeof(topic_2));

    subscribe.packet_id = 0x0c0d;
    subscribe.count = 2;
    subscribe.subscriptions = subscriptions;

    subscriptions[0].qos = 0;
    subscriptions[0].topic.buf = topic_1;
    subscriptions[0].topic.len = sizeof(topic_1);

    subscriptions[1].qos = 1;
    subscriptions[1].topic.buf = topic_2;
    subscriptions[1].topic.len = sizeof(topic_2);

    lmqtt_tx_buffer_subscribe(&state, &subscribe);

    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(267, bytes_written);

    ck_assert_int_eq((u8) '\x82', buf[0]);
    ck_assert_int_eq((u8) '\x88', buf[1]);
    ck_assert_int_eq((u8) '\x02', buf[2]);
    ck_assert_int_eq((u8) '\x0c', buf[3]);
    ck_assert_int_eq((u8) '\x0d', buf[4]);

    ck_assert_int_eq((u8) '\x00', buf[5]);
    ck_assert_int_eq((u8) '\x7f', buf[6]);
    ck_assert_int_eq((u8) 'x',    buf[7]);
    ck_assert_int_eq((u8) '\x00', buf[134]);

    ck_assert_int_eq((u8) '\x00', buf[135]);
    ck_assert_int_eq((u8) '\x81', buf[136]);
    ck_assert_int_eq((u8) 'y',    buf[137]);
    ck_assert_int_eq((u8) '\x01', buf[266]);
}
END_TEST

START_TEST(should_encode_pingreq)
{
    PREPARE;

    lmqtt_tx_buffer_pingreq(&state);

    ck_assert(&tx_buffer_finder_pingreq == state.finder);
    ck_assert_ptr_eq(0, state.data);

    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(2, bytes_written);

    ck_assert_int_eq(0xc0, buf[0]);
    ck_assert_int_eq(0x00, buf[1]);
}
END_TEST

START_TEST(should_encode_disconnect)
{
    PREPARE;

    lmqtt_tx_buffer_disconnect(&state);

    ck_assert(&tx_buffer_finder_disconnect == state.finder);
    ck_assert_ptr_eq(0, state.data);

    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(2, bytes_written);

    ck_assert_int_eq(0xe0, buf[0]);
    ck_assert_int_eq(0x00, buf[1]);
}
END_TEST

START_TCASE("Tx buffer recipes")
{
    ADD_TEST(should_encode_connect);
    ADD_TEST(should_encode_subscribe_to_one_topic);
    ADD_TEST(should_encode_subscribe_to_multiple_topics);
    ADD_TEST(should_encode_pingreq);
    ADD_TEST(should_encode_disconnect);
}
END_TCASE
