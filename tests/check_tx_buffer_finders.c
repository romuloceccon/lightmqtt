#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

#define PREPARE \
    void *data = 0; \
    u8 buf[512]; \
    lmqtt_tx_buffer_t state; \
    lmqtt_store_t store; \
    lmqtt_io_result_t res; \
    lmqtt_tx_buffer_callbacks_t tx_callbacks; \
    int bytes_written; \
    memset(buf, 0xcc, sizeof(buf)); \
    memset(&state, 0, sizeof(state)); \
    memset(&store, 0, sizeof(store)); \
    memset(&tx_callbacks, 0, sizeof(tx_callbacks)); \
    state.callbacks = &tx_callbacks; \
    state.callbacks_data = &data; \
    state.store = &store; \
    store.get_time = &test_time_get

int test_on_publish(void *data, lmqtt_publish_t *publish)
{
    *((void **) data) = publish;
    return 1;
}

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

    lmqtt_store_append(&store, LMQTT_CLASS_CONNECT, 0, &connect);

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

    lmqtt_store_append(&store, LMQTT_CLASS_SUBSCRIBE, 0x0a0b, &subscribe);

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

    lmqtt_store_append(&store, LMQTT_CLASS_SUBSCRIBE, 0x0c0d, &subscribe);

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

START_TEST(should_encode_unsubscribe_to_multiple_topics)
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

    subscriptions[0].topic.buf = topic_1;
    subscriptions[0].topic.len = sizeof(topic_1);

    subscriptions[1].topic.buf = topic_2;
    subscriptions[1].topic.len = sizeof(topic_2);

    lmqtt_store_append(&store, LMQTT_CLASS_UNSUBSCRIBE, 0x0c0d, &subscribe);

    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(265, bytes_written);

    ck_assert_int_eq((u8) '\xa2', buf[0]);
    ck_assert_int_eq((u8) '\x86', buf[1]);
    ck_assert_int_eq((u8) '\x02', buf[2]);
    ck_assert_int_eq((u8) '\x0c', buf[3]);
    ck_assert_int_eq((u8) '\x0d', buf[4]);

    ck_assert_int_eq((u8) '\x00', buf[5]);
    ck_assert_int_eq((u8) '\x7f', buf[6]);
    ck_assert_int_eq((u8) 'x',    buf[7]);
    ck_assert_int_eq((u8) 'x',    buf[133]);

    ck_assert_int_eq((u8) '\x00', buf[134]);
    ck_assert_int_eq((u8) '\x81', buf[135]);
    ck_assert_int_eq((u8) 'y',    buf[136]);
    ck_assert_int_eq((u8) 'y',    buf[264]);
}
END_TEST

START_TEST(should_encode_publish_with_qos_0)
{
    lmqtt_publish_t publish;
    int store_class;
    void *store_data;

    PREPARE;
    memset(&publish, 0, sizeof(publish));
    publish.packet_id = 0x0102;
    publish.topic.buf = "topic";
    publish.topic.len = strlen(publish.topic.buf);
    tx_callbacks.on_publish = &test_on_publish;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_0, 0x0102, &publish);

    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(11, bytes_written);

    ck_assert_int_eq(0, lmqtt_store_shift(&store, &store_class, &store_data));
    ck_assert_ptr_eq(0, store_data);
    ck_assert_ptr_eq(&publish, data);
}
END_TEST

START_TEST(should_encode_publish_with_qos_1)
{
    lmqtt_publish_t publish;
    int store_class;
    void *store_data;

    PREPARE;
    memset(&publish, 0, sizeof(publish));
    publish.packet_id = 0x0708;
    publish.retain = 1;
    publish.internal.encode_count++;
    publish.qos = 1;
    publish.topic.buf = "topic";
    publish.topic.len = strlen(publish.topic.buf);
    publish.payload.buf = "payload";
    publish.payload.len = strlen(publish.payload.buf);
    tx_callbacks.on_publish = &test_on_publish;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, 0x0708, &publish);

    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(18, bytes_written);

    ck_assert_uint_eq(0x3b, buf[0]);
    ck_assert_uint_eq(0x10, buf[1]);
    ck_assert_uint_eq(0x00, buf[2]);
    ck_assert_uint_eq(0x05, buf[3]);
    ck_assert_uint_eq((u8) 't', buf[4]);
    ck_assert_uint_eq((u8) 'c', buf[8]);
    ck_assert_uint_eq(0x07, buf[9]);
    ck_assert_uint_eq(0x08, buf[10]);
    ck_assert_uint_eq((u8) 'p', buf[11]);
    ck_assert_uint_eq((u8) 'd', buf[17]);

    ck_assert_int_eq(1, lmqtt_store_shift(&store, &store_class, &store_data));
    ck_assert_ptr_eq(&publish, store_data);
    ck_assert_ptr_eq(0, data);
}
END_TEST

START_TEST(should_increment_publish_encode_count_after_encode)
{
    lmqtt_publish_t publish;

    PREPARE;
    memset(&publish, 0, sizeof(publish));
    publish.packet_id = 0x0708;
    publish.qos = 1;
    publish.topic.buf = "topic";
    publish.topic.len = strlen(publish.topic.buf);
    publish.payload.buf = "payload";
    publish.payload.len = strlen(publish.payload.buf);

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, 0x0708, &publish);
    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_uint_eq(0x32, buf[0]);

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, 0x0708, &publish);
    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_uint_eq(0x3a, buf[0]);
}
END_TEST

START_TEST(should_encode_pubrel)
{
    lmqtt_publish_t publish;

    PREPARE;
    memset(&publish, 0, sizeof(publish));
    publish.packet_id = 0x0102;
    publish.topic.buf = "topic";
    publish.topic.len = strlen(publish.topic.buf);

    lmqtt_store_append(&store, LMQTT_CLASS_PUBREL, 0x0102, &publish);

    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(4, bytes_written);

    ck_assert_uint_eq(0x62, buf[0]);
    ck_assert_uint_eq(0x02, buf[1]);
    ck_assert_uint_eq(0x01, buf[2]);
    ck_assert_uint_eq(0x02, buf[3]);
}
END_TEST

START_TEST(should_encode_pingreq)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PINGREQ, 0, NULL);

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

    lmqtt_store_append(&store, LMQTT_CLASS_DISCONNECT, 0, NULL);

    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(2, bytes_written);

    ck_assert_int_eq(0xe0, buf[0]);
    ck_assert_int_eq(0x00, buf[1]);
}
END_TEST

START_TCASE("Tx buffer finders")
{
    ADD_TEST(should_encode_connect);
    ADD_TEST(should_encode_subscribe_to_one_topic);
    ADD_TEST(should_encode_subscribe_to_multiple_topics);
    ADD_TEST(should_encode_unsubscribe_to_multiple_topics);
    ADD_TEST(should_encode_publish_with_qos_0);
    ADD_TEST(should_encode_publish_with_qos_1);
    ADD_TEST(should_increment_publish_encode_count_after_encode);
    ADD_TEST(should_encode_pubrel);
    ADD_TEST(should_encode_pingreq);
    ADD_TEST(should_encode_disconnect);
}
END_TCASE
