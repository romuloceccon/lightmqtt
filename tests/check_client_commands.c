#include "check_lightmqtt.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    void *data;
    int succeeded;
} test_cb_result_t;

#define RX_BUFFER_SIZE 512
#define TX_BUFFER_SIZE 512

static test_socket_t ts;
static char topic[100];
static char payload[100];
static lmqtt_store_entry_t entries[16];
static u8 rx_buffer[RX_BUFFER_SIZE];
static u8 tx_buffer[TX_BUFFER_SIZE];

static void test_cb_result_set(void *cb_result, void *data, int succeeded)
{
    test_cb_result_t *result = (test_cb_result_t *) cb_result;
    result->data = data;
    result->succeeded = succeeded;
}

static lmqtt_write_result_t test_write_block(void *data, u8 *buf, int len,
    int *bytes_w)
{
    return LMQTT_WRITE_WOULD_BLOCK;
}

static void on_connect(void *data, lmqtt_connect_t *connect, int succeeded)
{
    test_cb_result_set(data, connect, succeeded);
}

static void on_subscribe(void *data, lmqtt_subscribe_t *subscribe, int succeeded)
{
    test_cb_result_set(data, subscribe, succeeded);
}

static void on_unsubscribe(void *data, lmqtt_subscribe_t *subscribe, int succeeded)
{
    test_cb_result_set(data, subscribe, succeeded);
}

static void on_publish(void *data, lmqtt_publish_t *publish, int succeeded)
{
    test_cb_result_set(data, publish, succeeded);
}

static int on_message_received(void *data, lmqtt_publish_t *publish)
{
    char *msg = data;
    sprintf(msg, "topic: %.*s, payload: %.*s", publish->topic.len,
        publish->topic.buf, publish->payload.len, publish->payload.buf);
    return 1;
}

static lmqtt_allocate_result_t on_publish_allocate_topic(void *data,
    lmqtt_publish_t *publish, int len)
{
    publish->topic.len = len;
    publish->topic.buf = topic;
    return LMQTT_ALLOCATE_SUCCESS;
}

static lmqtt_allocate_result_t on_publish_allocate_topic_block(void *data,
    lmqtt_publish_t *publish, int len)
{
    publish->topic.len = len;
    publish->topic.write = &test_write_block;
    return LMQTT_ALLOCATE_SUCCESS;
}

static lmqtt_allocate_result_t on_publish_allocate_payload(void *data,
    lmqtt_publish_t *publish, int len)
{
    publish->payload.len = len;
    publish->payload.buf = payload;
    return LMQTT_ALLOCATE_SUCCESS;
}

static lmqtt_connect_t connect;
static lmqtt_publish_t publish;
static char message_received[100];

static void do_init(lmqtt_client_t *client, long timeout)
{
    lmqtt_client_callbacks_t callbacks;
    lmqtt_client_buffers_t buffers;

    callbacks.read = test_socket_read;
    callbacks.write = test_socket_write;
    callbacks.data = &ts;
    callbacks.get_time = test_time_get;

    buffers.store_size = sizeof(entries);
    buffers.store = entries;
    buffers.rx_buffer_size = RX_BUFFER_SIZE;
    buffers.rx_buffer = rx_buffer;
    buffers.tx_buffer_size = TX_BUFFER_SIZE;
    buffers.tx_buffer = tx_buffer;

    lmqtt_client_initialize(client, &callbacks, &buffers);

    client->message_callbacks.on_publish = &on_message_received;
    client->message_callbacks.on_publish_allocate_topic =
        &on_publish_allocate_topic;
    client->message_callbacks.on_publish_allocate_payload =
        &on_publish_allocate_payload;
    client->message_callbacks.on_publish_data = message_received;

    lmqtt_client_set_default_timeout(client, timeout);
    test_socket_init(&ts);
}

static int do_connect(lmqtt_client_t *client, long keep_alive,
    int clean_session)
{
    memset(&connect, 0, sizeof(connect));
    connect.keep_alive = keep_alive;
    connect.clean_session = clean_session;
    connect.client_id.buf = "test";
    connect.client_id.len = 4;

    return lmqtt_client_connect(client, &connect);
}

static int do_connect_connack_process(lmqtt_client_t *client, long keep_alive)
{
    do_connect(client, keep_alive, 0);
    test_socket_append(&ts, TEST_CONNACK_SUCCESS);

    return LMQTT_IO_STATUS_BLOCK_DATA == client_process_output(client) &&
        LMQTT_IO_STATUS_BLOCK_CONN == client_process_input(client) &&
        TEST_CONNECT == test_socket_shift(&ts);
}

static int do_init_connect_connack_process(lmqtt_client_t *client,
    long keep_alive, long timeout)
{
    do_init(client, timeout);
    return do_connect_connack_process(client, keep_alive);
}

static int do_connect_process(lmqtt_client_t *client, long keep_alive)
{
    do_connect(client, keep_alive, 0);

    return LMQTT_IO_STATUS_BLOCK_DATA == client_process_output(client) &&
        TEST_CONNECT == test_socket_shift(&ts);
}

static int do_init_connect_process(lmqtt_client_t *client, long keep_alive,
    long timeout)
{
    do_init(client, timeout);
    return do_connect_process(client, keep_alive);
}

static int do_publish(lmqtt_client_t *client, int qos)
{
    memset(&publish, 0, sizeof(publish));
    publish.qos = qos;
    publish.topic.buf = "topic";
    publish.topic.len = strlen(publish.topic.buf);
    publish.payload.buf = "payload";
    publish.payload.len = strlen(publish.payload.buf);

    return lmqtt_client_publish(client, &publish);
}

static int close_read_buf(lmqtt_client_t *client)
{
    int previous_len = ts.read_buf.len;
    int result;

    ts.read_buf.len = ts.read_buf.available_len;
    result = LMQTT_IO_STATUS_READY == client_process_input(client);
    ts.read_buf.len = previous_len;

    return result;
}

static void check_resend_packets_with_clean_session(int first, int second)
{
    lmqtt_client_t client;

    do_init(&client, 3);

    do_connect(&client, 5, first);
    client_process_output(&client);
    ck_assert_int_eq(TEST_CONNECT, test_socket_shift(&ts));

    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    client_process_input(&client);

    do_publish(&client, 1);
    client_process_output(&client);
    ck_assert_int_eq(TEST_PUBLISH, test_socket_shift(&ts));

    close_read_buf(&client);

    do_connect(&client, 5, second);
    client_process_output(&client);

    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    client_process_input(&client);

    client_process_output(&client);
    ck_assert_int_eq(TEST_CONNECT, test_socket_shift(&ts));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}

static void check_connect_and_receive_message(lmqtt_client_t *client,
    int clean_session, int packet_id)
{
    do_connect(client, 5, clean_session);
    client_process_output(client);
    test_socket_shift(&ts);

    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    test_socket_append_param(&ts, TEST_PUBLISH_QOS_2, packet_id);
    memset(message_received, 0, sizeof(message_received));
    client_process_input(client);
}

START_TEST(should_initialize_client)
{
    lmqtt_client_t client;
    lmqtt_client_callbacks_t callbacks;
    lmqtt_client_buffers_t buffers;

    memset(&client, -1, sizeof(client));

    callbacks.read = test_socket_read;
    callbacks.write = test_socket_write;
    callbacks.data = &ts;
    callbacks.get_time = test_time_get;

    buffers.store_size = sizeof(entries);
    buffers.store = entries;

    lmqtt_client_initialize(&client, &callbacks, &buffers);

    ck_assert_ptr_eq(&ts, client.callbacks.data);
    ck_assert(client.callbacks.read);
    ck_assert(client.callbacks.write);
    ck_assert(client.main_store.get_time);
    ck_assert(client.connect_store.get_time);
    ck_assert_int_eq(0, client.failed);
    ck_assert_ptr_eq(&client.connect_store, client.rx_state.store);
    ck_assert_ptr_eq(&client.connect_store, client.tx_state.store);
}
END_TEST

START_TEST(should_prepare_connect_after_initialize)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;

    do_init(&client, 5);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;

    ck_assert_int_eq(1, lmqtt_client_connect(&client, &connect));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(TEST_CONNECT, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_not_prepare_connect_twice)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;

    do_init(&client, 5);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;

    ck_assert_int_eq(1, lmqtt_client_connect(&client, &connect));
    client_process_output(&client);
    ck_assert_int_eq(TEST_CONNECT, test_socket_shift(&ts));

    ck_assert_int_eq(0, lmqtt_client_connect(&client, &connect));
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_not_prepare_invalid_connect)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;

    do_init(&client, 5);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 0; /* invalid; should have a non-empty client_id */

    ck_assert_int_eq(0, lmqtt_client_connect(&client, &connect));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_receive_connack_after_connect)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;
    test_cb_result_t cb_result = { 0, 0 };

    do_init(&client, 5);

    lmqtt_client_set_on_connect(&client, on_connect, &cb_result);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;
    lmqtt_client_connect(&client, &connect);
    client_process_output(&client);

    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_input(&client));
    ck_assert_ptr_eq(&connect, cb_result.data);
    ck_assert_int_eq(1, cb_result.succeeded);

    /* should not receive connack twice */
    cb_result.data = 0;
    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_input(&client));
    ck_assert_ptr_eq(0, cb_result.data);
}
END_TEST

START_TEST(should_call_connect_callback_on_connect_failure)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;
    test_cb_result_t cb_result = { 0, 0 };

    do_init(&client, 5);

    lmqtt_client_set_on_connect(&client, on_connect, &cb_result);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;
    lmqtt_client_connect(&client, &connect);
    client_process_output(&client);

    cb_result.data = 0;
    test_socket_append(&ts, TEST_CONNACK_FAILURE);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_input(&client));

    ck_assert_ptr_eq(&connect, cb_result.data);
    ck_assert_int_eq(0, cb_result.succeeded);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
}
END_TEST

START_TEST(should_not_receive_connack_before_connect)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;
    test_cb_result_t cb_result = { 0, 0 };

    do_init(&client, 5);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;
    lmqtt_client_set_on_connect(&client, on_connect, &cb_result);

    cb_result.data = 0;
    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_input(&client));
    ck_assert_ptr_eq(0, cb_result.data);
}
END_TEST

START_TEST(should_subscribe)
{
    lmqtt_client_t client;
    lmqtt_subscribe_t subscribe;
    lmqtt_subscription_t subscription;
    test_cb_result_t cb_result = { 0, 0 };

    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    lmqtt_client_set_on_subscribe(&client, on_subscribe, &cb_result);

    memset(&subscribe, 0, sizeof(subscribe));
    memset(&subscription, 0, sizeof(subscription));
    subscribe.count = 1;
    subscribe.subscriptions = &subscription;
    subscription.qos = 0;
    subscription.topic.buf = "test";
    subscription.topic.len = strlen(subscription.topic.buf);

    ck_assert_int_eq(1, lmqtt_client_subscribe(&client, &subscribe));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(TEST_SUBSCRIBE, test_socket_shift(&ts));

    test_socket_append(&ts, TEST_SUBACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_input(&client));
    ck_assert_ptr_eq(&subscribe, cb_result.data);
    ck_assert_int_eq(1, cb_result.succeeded);
}
END_TEST

START_TEST(should_unsubscribe)
{
    lmqtt_client_t client;
    lmqtt_subscribe_t subscribe;
    lmqtt_subscription_t subscription;
    test_cb_result_t cb_result = { 0, 0 };

    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    lmqtt_client_set_on_unsubscribe(&client, on_unsubscribe, &cb_result);

    memset(&subscribe, 0, sizeof(subscribe));
    memset(&subscription, 0, sizeof(subscription));
    subscribe.count = 1;
    subscribe.subscriptions = &subscription;
    subscription.topic.buf = "test";
    subscription.topic.len = strlen(subscription.topic.buf);

    ck_assert_int_eq(1, lmqtt_client_unsubscribe(&client, &subscribe));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(TEST_UNSUBSCRIBE, test_socket_shift(&ts));

    test_socket_append(&ts, TEST_UNSUBACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_input(&client));
    ck_assert_ptr_eq(&subscribe, cb_result.data);
    ck_assert_int_eq(1, cb_result.succeeded);
}
END_TEST

START_TEST(should_assign_packet_ids_to_subscribe)
{
    lmqtt_client_t client;
    lmqtt_subscribe_t subscribe[3];
    lmqtt_subscription_t subscriptions[3];
    void *data;
    lmqtt_store_entry_callback_t cb;
    int i;
    lmqtt_store_value_t value;
    int class;

    memset(&subscribe, 0, sizeof(subscribe));
    memset(&subscriptions, 0, sizeof(subscriptions));
    for (i = 0; i < 3; i++) {
        subscribe[i].count = 1;
        subscribe[i].subscriptions = &subscriptions[i];
        subscriptions[i].topic.buf = "test";
        subscriptions[i].topic.len = strlen(subscriptions[i].topic.buf);
    }

    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    ck_assert_int_eq(1, lmqtt_client_subscribe(&client, &subscribe[0]));
    ck_assert_int_eq(1, lmqtt_client_subscribe(&client, &subscribe[1]));
    ck_assert_int_eq(1, lmqtt_client_subscribe(&client, &subscribe[2]));

    lmqtt_store_get_at(&client.main_store, 0, &class, &value);
    ck_assert_uint_eq(0, value.packet_id);
    lmqtt_store_get_at(&client.main_store, 1, &class, &value);
    ck_assert_uint_eq(1, value.packet_id);
    lmqtt_store_get_at(&client.main_store, 2, &class, &value);
    ck_assert_uint_eq(2, value.packet_id);

    lmqtt_store_mark_current(&client.main_store);
    lmqtt_store_mark_current(&client.main_store);
    lmqtt_store_mark_current(&client.main_store);

    ck_assert_int_eq(1, lmqtt_store_pop_marked_by(&client.main_store,
        LMQTT_CLASS_SUBSCRIBE, 0, NULL));
    ck_assert_int_eq(1, lmqtt_store_pop_marked_by(&client.main_store,
        LMQTT_CLASS_SUBSCRIBE, 1, NULL));
    ck_assert_int_eq(1, lmqtt_store_pop_marked_by(&client.main_store,
        LMQTT_CLASS_SUBSCRIBE, 2, NULL));
}
END_TEST

START_TEST(should_not_subscribe_with_invalid_packet)
{
    lmqtt_client_t client;
    lmqtt_subscribe_t subscribe;

    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    memset(&subscribe, 0, sizeof(subscribe));

    ck_assert_int_eq(0, lmqtt_client_subscribe(&client, &subscribe));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_not_subscribe_with_full_store)
{
    lmqtt_client_t client;
    lmqtt_subscribe_t subscribe;
    lmqtt_subscription_t subscription;
    int i;

    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    memset(&subscribe, 0, sizeof(subscribe));
    memset(&subscription, 0, sizeof(subscription));
    subscribe.count = 1;
    subscribe.subscriptions = &subscription;
    subscription.qos = 0;
    subscription.topic.buf = "test";
    subscription.topic.len = strlen(subscription.topic.buf);

    for (i = 0; lmqtt_store_append(&client.main_store, LMQTT_CLASS_PINGREQ,
            NULL); i++)
        ;

    ck_assert_int_eq(0, lmqtt_client_subscribe(&client, &subscribe));
}
END_TEST

START_TEST(should_publish_with_qos_0)
{
    lmqtt_client_t client;
    test_cb_result_t cb_result = { 0, 0 };
    lmqtt_store_value_t value;
    int class;

    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    lmqtt_client_set_on_publish(&client, on_publish, &cb_result);
    lmqtt_store_get_id(&client.main_store);

    ck_assert_int_eq(1, do_publish(&client, 0));
    /* should NOT assign id to QoS 0 packet */
    lmqtt_store_get_at(&client.main_store, 0, &class, &value);
    ck_assert_uint_eq(0, value.packet_id);

    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(TEST_PUBLISH, test_socket_shift(&ts));

    ck_assert_ptr_eq(&publish, cb_result.data);
    ck_assert_int_eq(1, cb_result.succeeded);
}
END_TEST

START_TEST(should_publish_with_qos_1)
{
    lmqtt_client_t client;
    test_cb_result_t cb_result = { 0, 0 };
    lmqtt_store_value_t value;
    int class;

    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    lmqtt_client_set_on_publish(&client, on_publish, &cb_result);
    lmqtt_store_get_id(&client.main_store);

    ck_assert_int_eq(1, do_publish(&client, 1));
    /* should assign id to QoS 1 packet */
    lmqtt_store_get_at(&client.main_store, 0, &class, &value);
    ck_assert_uint_eq(1, value.packet_id);

    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(TEST_PUBLISH, test_socket_shift(&ts));

    ck_assert_ptr_eq(0, cb_result.data);
    ck_assert_int_eq(0, cb_result.succeeded);

    test_socket_append_param(&ts, TEST_PUBACK, 1);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_input(&client));

    ck_assert_ptr_eq(&publish, cb_result.data);
    ck_assert_int_eq(1, cb_result.succeeded);
}
END_TEST

START_TEST(should_publish_with_qos_2)
{
    lmqtt_client_t client;
    test_cb_result_t cb_result = { 0, 0 };
    lmqtt_store_value_t value;
    int class;

    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    lmqtt_client_set_on_publish(&client, on_publish, &cb_result);

    lmqtt_store_get_id(&client.main_store);

    ck_assert_int_eq(1, do_publish(&client, 2));
    /* should assign id to QoS 2 packet */
    lmqtt_store_get_at(&client.main_store, 0, &class, &value);
    ck_assert_uint_eq(1, value.packet_id);

    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(TEST_PUBLISH, test_socket_shift(&ts));

    ck_assert_ptr_eq(0, cb_result.data);
    ck_assert_int_eq(0, cb_result.succeeded);

    test_socket_append_param(&ts, TEST_PUBREC, 1);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_input(&client));

    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(TEST_PUBREL, test_socket_shift(&ts));

    ck_assert_ptr_eq(0, cb_result.data);
    ck_assert_int_eq(0, cb_result.succeeded);

    test_socket_append_param(&ts, TEST_PUBCOMP, 1);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_input(&client));

    ck_assert_ptr_eq(&publish, cb_result.data);
    ck_assert_int_eq(1, cb_result.succeeded);
}
END_TEST

START_TEST(should_not_publish_invalid_packet)
{
    lmqtt_client_t client;
    lmqtt_publish_t publish;

    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    memset(&publish, 0, sizeof(publish));

    ck_assert_int_eq(0, lmqtt_client_publish(&client, &publish));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_send_pingreq_after_timeout)
{
    lmqtt_client_t client;
    long secs, nsecs;

    test_time_set(10, 0);
    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    test_time_set(15, 0);
    ck_assert_int_eq(1, lmqtt_client_get_timeout(&client, &secs, &nsecs));
    ck_assert_int_eq(0, secs);

    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(TEST_PINGREQ, test_socket_shift(&ts));

    ck_assert_int_eq(1, lmqtt_client_get_timeout(&client, &secs, &nsecs));
    ck_assert_int_eq(3, secs);
}
END_TEST

START_TEST(should_not_send_pingreq_before_timeout)
{
    lmqtt_client_t client;
    long secs, nsecs;

    test_time_set(10, 0);
    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    test_time_set(14, 0);
    ck_assert_int_eq(1, lmqtt_client_get_timeout(&client, &secs, &nsecs));
    ck_assert_int_eq(1, secs);

    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(-1, test_socket_shift(&ts));

    ck_assert_int_eq(1, lmqtt_client_get_timeout(&client, &secs, &nsecs));
    ck_assert_int_eq(1, secs);
}
END_TEST

START_TEST(should_not_send_pingreq_with_response_pending)
{
    lmqtt_client_t client;
    long secs, nsecs;

    test_time_set(10, 0);
    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    test_time_set(16, 0);
    client_keep_alive(&client);
    client_process_output(&client);
    ck_assert_int_eq(TEST_PINGREQ, test_socket_shift(&ts));

    test_time_set(17, 0);
    client_keep_alive(&client);
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_send_pingreq_after_pingresp)
{
    lmqtt_client_t client;
    long secs, nsecs;

    test_time_set(10, 0);
    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    test_time_set(16, 0);
    client_keep_alive(&client);
    client_process_output(&client);
    ck_assert_int_eq(TEST_PINGREQ, test_socket_shift(&ts));
    test_socket_append(&ts, TEST_PINGRESP);
    client_process_input(&client);

    test_time_set(22, 0);
    client_keep_alive(&client);
    client_process_output(&client);
    ck_assert_int_eq(TEST_PINGREQ, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_fail_client_after_pingreq_timeout)
{
    lmqtt_client_t client;
    long secs, nsecs;

    test_time_set(10, 0);
    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    test_time_set(15, 0);
    ck_assert_int_eq(1, lmqtt_client_get_timeout(&client, &secs, &nsecs));
    ck_assert_int_eq(0, secs);

    client_keep_alive(&client);
    client_process_output(&client);
    ck_assert_int_eq(TEST_PINGREQ, test_socket_shift(&ts));

    test_time_set(20, 0);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_keep_alive(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_output(&client));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_disconnect)
{
    lmqtt_client_t client;

    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    ck_assert_int_eq(1, lmqtt_client_disconnect(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(TEST_DISCONNECT, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_not_send_pingreq_after_disconnect)
{
    lmqtt_client_t client;

    test_time_set(10, 0);
    do_init_connect_connack_process(&client, 5, 3);

    test_time_set(16, 0);
    ck_assert_int_eq(1, lmqtt_client_disconnect(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));

    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(TEST_DISCONNECT, test_socket_shift(&ts));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_not_disconnect_before_connack)
{
    lmqtt_client_t client;

    do_init_connect_process(&client, 5, 3);

    ck_assert_int_eq(0, lmqtt_client_disconnect(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_not_send_pingreq_after_failure)
{
    lmqtt_client_t client;

    test_time_set(10, 0);
    do_init_connect_process(&client, 5, 0);

    test_socket_append(&ts, TEST_SUBACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_input(&client));

    test_time_set(16, 0);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_keep_alive(&client));
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_not_send_pingreq_before_connack)
{
    lmqtt_client_t client;

    test_time_set(10, 0);
    do_init_connect_process(&client, 5, 0);

    test_time_set(16, 0);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_fail_client_after_connection_timeout)
{
    lmqtt_client_t client;

    test_time_set(10, 0);
    do_init_connect_process(&client, 5, 3);

    test_time_set(14, 0);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_keep_alive(&client));
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_not_fail_client_before_connection_timeout)
{
    lmqtt_client_t client;

    test_time_set(10, 0);
    do_init_connect_process(&client, 5, 7);

    test_time_set(16, 0);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_not_send_pingreq_with_zeroed_keep_alive)
{
    lmqtt_client_t client;

    test_time_set(10, 0);
    do_init_connect_process(&client, 0, 0);

    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_input(&client));

    test_time_set(11, 0);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_clean_pingreq_and_disconnect_packets_after_close)
{
    lmqtt_client_t client;
    int class;
    lmqtt_store_value_t value;

    test_time_set(10, 0);
    do_init_connect_connack_process(&client, 5, 3);

    test_time_set(16, 0);

    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));
    ck_assert_int_eq(1, do_publish(&client, 0));
    ck_assert_int_eq(1, lmqtt_client_disconnect(&client));
    ck_assert_int_eq(3, lmqtt_store_count(&client.main_store));

    ts.read_buf.len = 0;
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_input(&client));

    ck_assert_int_eq(1, lmqtt_store_count(&client.main_store));
    lmqtt_store_shift(&client.main_store, &class, &value);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH_0, class);
    ck_assert_ptr_eq(&publish, value.value);
}
END_TEST

START_TEST(should_close_encoder_after_socket_close)
{
    lmqtt_client_t client;

    do_init_connect_connack_process(&client, 5, 3);
    ck_assert_int_eq(1, do_publish(&client, 0));

    ts.read_buf.len = 0;
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_input(&client));

    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
    ck_assert_int_eq(1, lmqtt_store_count(&client.main_store));
}
END_TEST

START_TEST(should_close_encoder_after_connect_failure)
{
    lmqtt_client_t client;

    do_init_connect_process(&client, 5, 3);

    test_socket_append(&ts, TEST_CONNACK_FAILURE);

    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_input(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
}
END_TEST

START_TEST(should_reconnect_after_close)
{
    lmqtt_client_t client;

    do_init_connect_connack_process(&client, 5, 3);

    ts.read_buf.len = 0;
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_input(&client));

    ts.read_buf.len = sizeof(ts.read_buf.buf);
    ck_assert_int_eq(1, do_connect_connack_process(&client, 5));
}
END_TEST

START_TEST(should_reconnect_after_disconnect)
{
    lmqtt_client_t client;

    do_init_connect_connack_process(&client, 5, 3);

    ck_assert_int_eq(1, lmqtt_client_disconnect(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(TEST_DISCONNECT, test_socket_shift(&ts));

    ck_assert_int_eq(1, do_connect_connack_process(&client, 5));
}
END_TEST

START_TEST(should_reset_output_buffer_on_reconnect)
{
    lmqtt_client_t client;

    do_init_connect_connack_process(&client, 5, 3);
    ck_assert_int_eq(1, do_publish(&client, 0));

    /* Fill tx buffer, but do not flush it */
    ts.write_buf.available_len = ts.write_buf.pos;
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_output(&client));

    /* Close remote side of connection */
    ts.read_buf.len = 0;
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_input(&client));

    memset(&connect, 0, sizeof(connect));
    connect.client_id.buf = "test";
    connect.client_id.len = 4;
    ck_assert_int_eq(1, lmqtt_client_connect(&client, &connect));

    ts.write_buf.available_len = ts.write_buf.len;
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    /* PUBLISH (QoS 0) packet should have been discarded */
    ck_assert_int_eq(TEST_CONNECT, test_socket_shift(&ts));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_reset_input_buffer_on_reconnect)
{
    lmqtt_client_t client;

    do_init(&client, 3);

    client.message_callbacks.on_publish_allocate_topic =
        &on_publish_allocate_topic_block;

    check_connect_and_receive_message(&client, 0, 0x0304);

    ck_assert_int_eq(1, lmqtt_client_disconnect(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(TEST_DISCONNECT, test_socket_shift(&ts));

    do_connect(&client, 5, 0);
    test_socket_append(&ts, TEST_CONNACK_SUCCESS);

    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_input(&client));
    ck_assert_int_eq(TEST_CONNECT, test_socket_shift(&ts));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_reset_decoder_on_reconnect)
{
    lmqtt_client_t client;

    do_init_connect_connack_process(&client, 5, 3);
    ck_assert_int_eq(1, do_publish(&client, 1));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));

    test_socket_append(&ts, TEST_PUBACK);

    /* Close remote side of connection after reading partial buffer */
    ts.read_buf.available_len -= 1;
    ts.read_buf.len = ts.read_buf.available_len;
    ts.test_pos_read -= 1;
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_input(&client));

    memset(&connect, 0, sizeof(connect));
    connect.client_id.buf = "test";
    connect.client_id.len = 4;
    ck_assert_int_eq(1, lmqtt_client_connect(&client, &connect));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));

    /* should discard partial buffer */
    ts.read_buf.len = sizeof(ts.read_buf.buf);
    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_input(&client));
}
END_TEST

START_TEST(should_finalize_client)
{
    lmqtt_client_t client;
    lmqtt_subscribe_t subscribe;
    lmqtt_subscribe_t unsubscribe;
    lmqtt_publish_t publish;
    lmqtt_subscription_t subscription;
    test_cb_result_t cb_result_1 = { 0, -1 };
    test_cb_result_t cb_result_2 = { 0, -1 };
    test_cb_result_t cb_result_3 = { 0, -1 };

    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    lmqtt_client_set_on_subscribe(&client, on_subscribe, &cb_result_1);
    lmqtt_client_set_on_unsubscribe(&client, on_unsubscribe, &cb_result_2);
    lmqtt_client_set_on_publish(&client, on_publish, &cb_result_3);

    memset(&subscribe, 0, sizeof(subscribe));
    memset(&unsubscribe, 0, sizeof(unsubscribe));
    memset(&subscription, 0, sizeof(subscription));
    subscribe.count = 1;
    subscribe.subscriptions = &subscription;
    unsubscribe.count = 1;
    unsubscribe.subscriptions = &subscription;
    subscription.qos = 0;
    subscription.topic.buf = "test";
    subscription.topic.len = strlen(subscription.topic.buf);
    memset(&publish, 0, sizeof(publish));
    publish.topic.buf = "topic";
    publish.topic.len = strlen(publish.topic.buf);

    ck_assert_int_eq(1, lmqtt_client_subscribe(&client, &subscribe));
    ck_assert_int_eq(1, lmqtt_client_unsubscribe(&client, &unsubscribe));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    /* publish after process_output; otherwise qos 0 packet will be dropped from
       queue immediattely after being sent */
    ck_assert_int_eq(1, lmqtt_client_publish(&client, &publish));

    lmqtt_client_finalize(&client);
    ck_assert_ptr_eq(&subscribe, cb_result_1.data);
    ck_assert_int_eq(0, cb_result_1.succeeded);
    ck_assert_ptr_eq(&unsubscribe, cb_result_2.data);
    ck_assert_int_eq(0, cb_result_2.succeeded);
    ck_assert_ptr_eq(&publish, cb_result_3.data);
    ck_assert_int_eq(0, cb_result_3.succeeded);
}
END_TEST

START_TEST(should_finalize_client_before_connack)
{
    lmqtt_client_t client;
    test_cb_result_t cb_result = { 0, -1 };

    do_init(&client, 5);

    lmqtt_client_set_on_connect(&client, &on_connect, &cb_result);
    ck_assert(do_connect_process(&client, 3));

    lmqtt_client_finalize(&client);
    ck_assert_ptr_eq(&connect, cb_result.data);
    ck_assert_int_eq(0, cb_result.succeeded);
}
END_TEST

START_TEST(should_finalize_client_after_partial_decode)
{
    lmqtt_client_t client;
    test_cb_result_t cb_result = { 0, -1 };

    do_init(&client, 5);

    lmqtt_client_set_on_connect(&client, &on_connect, &cb_result);
    ck_assert(do_connect_process(&client, 3));

    test_socket_append(&ts, TEST_CONNACK_FAILURE);

    ts.read_buf.available_len -= 1;
    ts.read_buf.len = ts.read_buf.available_len;
    ts.test_pos_read -= 1;
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_input(&client));

    lmqtt_client_finalize(&client);
    ck_assert_ptr_eq(&connect, cb_result.data);
    ck_assert_int_eq(0, cb_result.succeeded);
}
END_TEST

START_TEST(should_not_reconnect_after_finalize)
{
    lmqtt_client_t client;

    ck_assert_int_eq(1, do_init_connect_connack_process(&client, 5, 3));

    lmqtt_client_finalize(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;

    ck_assert_int_eq(0, lmqtt_client_connect(&client, &connect));
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_output(&client));
}
END_TEST

START_TEST(should_wait_connack_to_resend_packets_from_previous_connection)
{
    lmqtt_client_t client;

    do_init_connect_connack_process(&client, 5, 3);
    ck_assert(do_publish(&client, 1));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(TEST_PUBLISH, test_socket_shift(&ts));

    ck_assert(close_read_buf(&client));
    ck_assert(do_connect_process(&client, 5));
    ck_assert_int_eq(-1, test_socket_shift(&ts));

    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_input(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(TEST_PUBLISH, test_socket_shift(&ts));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_wait_connack_to_send_unsent_packets_from_previous_connection)
{
    lmqtt_client_t client;

    do_init_connect_connack_process(&client, 5, 3);
    ck_assert(do_publish(&client, 1));
    ck_assert(close_read_buf(&client));
    ck_assert(do_connect_process(&client, 5));
    ck_assert_int_eq(-1, test_socket_shift(&ts));

    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, client_process_input(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_output(&client));
    ck_assert_int_eq(TEST_PUBLISH, test_socket_shift(&ts));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_not_resend_connect_from_previous_connection)
{
    lmqtt_client_t client;
    test_cb_result_t cb_result = { 0, -1 };
    lmqtt_connect_t connect_2;

    do_init(&client, 3);

    memset(&connect_2, 0, sizeof(connect_2));
    connect_2.keep_alive = 5;
    connect_2.clean_session = 1;

    lmqtt_client_set_on_connect(&client, &on_connect, &cb_result);
    lmqtt_client_connect(&client, &connect_2);

    ck_assert(close_read_buf(&client));
    ck_assert(do_connect_process(&client, 5));

    ck_assert_int_eq(-1, test_socket_shift(&ts));
    ck_assert_ptr_eq(&connect_2, cb_result.data);
    ck_assert_int_eq(0, cb_result.succeeded);
}
END_TEST

START_TEST(should_not_resend_packets_if_previous_connection_had_clean_session_set)
{
    check_resend_packets_with_clean_session(1, 0);
}
END_TEST

START_TEST(should_not_resend_packets_if_new_connection_has_clean_session_set)
{
    check_resend_packets_with_clean_session(0, 1);
}
END_TEST

START_TEST(should_preserve_non_clean_session_ids_after_reconnect)
{
    lmqtt_client_t client;

    do_init(&client, 3);

    check_connect_and_receive_message(&client, 0, 0x0304);
    ck_assert_str_eq("topic: X, payload: X", message_received);

    close_read_buf(&client);

    check_connect_and_receive_message(&client, 0, 0x0304);
    ck_assert_str_eq("", message_received);
}
END_TEST

START_TEST(should_not_preserve_clean_session_ids_after_reconnect)
{
    lmqtt_client_t client;

    do_init(&client, 3);

    check_connect_and_receive_message(&client, 1, 0x0304);
    ck_assert_str_eq("topic: X, payload: X", message_received);

    close_read_buf(&client);

    check_connect_and_receive_message(&client, 0, 0x0304);
    ck_assert_str_eq("topic: X, payload: X", message_received);
}
END_TEST

START_TCASE("Client commands")
{
    ADD_TEST(should_initialize_client);
    ADD_TEST(should_prepare_connect_after_initialize);
    ADD_TEST(should_not_prepare_connect_twice);
    ADD_TEST(should_not_prepare_invalid_connect);
    ADD_TEST(should_receive_connack_after_connect);
    ADD_TEST(should_call_connect_callback_on_connect_failure);
    ADD_TEST(should_not_receive_connack_before_connect);

    ADD_TEST(should_subscribe);
    ADD_TEST(should_unsubscribe);
    ADD_TEST(should_assign_packet_ids_to_subscribe);
    ADD_TEST(should_not_subscribe_with_invalid_packet);
    ADD_TEST(should_not_subscribe_with_full_store);

    ADD_TEST(should_publish_with_qos_0);
    ADD_TEST(should_publish_with_qos_1);
    ADD_TEST(should_publish_with_qos_2);
    ADD_TEST(should_not_publish_invalid_packet);

    ADD_TEST(should_send_pingreq_after_timeout);
    ADD_TEST(should_not_send_pingreq_before_timeout);
    ADD_TEST(should_not_send_pingreq_with_response_pending);
    ADD_TEST(should_send_pingreq_after_pingresp);
    ADD_TEST(should_fail_client_after_pingreq_timeout);
    ADD_TEST(should_disconnect);

    ADD_TEST(should_not_send_pingreq_after_disconnect);
    ADD_TEST(should_not_disconnect_before_connack);
    ADD_TEST(should_not_send_pingreq_after_failure);
    ADD_TEST(should_not_send_pingreq_before_connack);
    ADD_TEST(should_fail_client_after_connection_timeout);
    ADD_TEST(should_not_fail_client_before_connection_timeout);
    ADD_TEST(should_not_send_pingreq_with_zeroed_keep_alive);

    ADD_TEST(should_clean_pingreq_and_disconnect_packets_after_close);
    ADD_TEST(should_close_encoder_after_socket_close);
    ADD_TEST(should_close_encoder_after_connect_failure);
    ADD_TEST(should_reconnect_after_close);
    ADD_TEST(should_reconnect_after_disconnect);
    ADD_TEST(should_reset_output_buffer_on_reconnect);
    ADD_TEST(should_reset_input_buffer_on_reconnect);
    ADD_TEST(should_reset_decoder_on_reconnect);
    ADD_TEST(should_finalize_client);
    ADD_TEST(should_finalize_client_before_connack);
    ADD_TEST(should_finalize_client_after_partial_decode);
    ADD_TEST(should_not_reconnect_after_finalize);

    ADD_TEST(should_wait_connack_to_resend_packets_from_previous_connection);
    ADD_TEST(should_wait_connack_to_send_unsent_packets_from_previous_connection);
    ADD_TEST(should_not_resend_connect_from_previous_connection);
    ADD_TEST(should_not_resend_packets_if_previous_connection_had_clean_session_set);
    ADD_TEST(should_not_resend_packets_if_new_connection_has_clean_session_set);

    ADD_TEST(should_preserve_non_clean_session_ids_after_reconnect);
    ADD_TEST(should_not_preserve_clean_session_ids_after_reconnect);
}
END_TCASE
