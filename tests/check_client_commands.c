#include "check_lightmqtt.h"
#include <assert.h>

#include "../src/lmqtt_io.c"

typedef enum {
    TEST_CONNECT = 5000,
    TEST_SUBSCRIBE,
    TEST_UNSUBSCRIBE,
    TEST_PINGREQ,
    TEST_DISCONNECT
} test_type_request_t;

typedef enum {
    TEST_CONNACK_SUCCESS = 5100,
    TEST_CONNACK_FAILURE,
    TEST_SUBACK_SUCCESS,
    TEST_UNSUBACK_SUCCESS,
    TEST_PINGRESP
} test_type_response_t;

typedef struct {
    test_buffer_t read_buf;
    test_buffer_t write_buf;
    int test_pos_read;
    int test_pos_write;
} test_socket_t;

static test_socket_t test_socket;

static lmqtt_io_result_t test_socket_read(void *data, u8 *buf, int buf_len,
    int *bytes_read)
{
    test_socket_t *sock = (test_socket_t *) data;
    return test_buffer_read(&sock->read_buf, buf, buf_len, bytes_read);
}

static lmqtt_io_result_t test_socket_write(void *data, u8 *buf, int buf_len,
    int *bytes_written)
{
    test_socket_t *sock = (test_socket_t *) data;
    return test_buffer_write(&sock->write_buf, buf, buf_len, bytes_written);
}

static void test_socket_init_with_client(lmqtt_client_t *client)
{
    memset(&test_socket, 0, sizeof(test_socket));
    test_socket.write_buf.len = sizeof(test_socket.write_buf.buf);
    test_socket.write_buf.available_len = test_socket.write_buf.len;

    if (client) {
        client->get_time = test_time_get;
        client->store.get_time = test_time_get;
        client->read = test_socket_read;
        client->write = test_socket_write;
        client->data = &test_socket;
    }
}

static void test_socket_append(int val)
{
    test_buffer_t *buf = &test_socket.read_buf;

    char *src = NULL;
    int len = 0;

    switch (val) {
        case TEST_CONNACK_SUCCESS:
            src = "\x20\x02\x00\x00";
            len = 4;
            break;
        case TEST_CONNACK_FAILURE:
            src = "\x20\x02\x00\x01";
            len = 4;
            break;
        case TEST_SUBACK_SUCCESS:
            src = "\x90\x03\x00\x00\x00";
            len = 5;
            break;
        case TEST_UNSUBACK_SUCCESS:
            src = "\xb0\x02\x00\x00";
            len = 4;
            break;
        case TEST_PINGRESP:
            src = "\xd0\x00";
            len = 2;
            break;
    }

    if (src) {
        memcpy(&buf->buf[test_socket.test_pos_read], src, len);
        test_socket.test_pos_read += len;
        buf->available_len += len;
        buf->len += len;
    }
}

static int test_socket_shift()
{
    test_buffer_t *buf = &test_socket.write_buf;

    test_type_request_t result = -4; /* invalid command */
    u8 cmd;
    u8 remain_len;
    int len;
    int available = buf->pos - test_socket.test_pos_write;

    if (available <= 0)
        return -1; /* eof */
    if (available < 2)
        return -2; /* partial buffer */

    cmd = buf->buf[test_socket.test_pos_write];
    remain_len = buf->buf[test_socket.test_pos_write + 1];

    if (remain_len & 0x80)
        return -3; /* invalid remaining length (for simplicity we do not
                      implement here decoding for values larger than 127) */

    len = 2 + remain_len;
    if (available < len)
        return -2; /* partial buffer */

    switch (cmd) {
        case 0x10:
            result = TEST_CONNECT;
            break;
        case 0x82:
            result = TEST_SUBSCRIBE;
            break;
        case 0xa2:
            result = TEST_UNSUBSCRIBE;
            break;
        case 0xc0:
            result = TEST_PINGREQ;
            break;
        case 0xe0:
            result = TEST_DISCONNECT;
            break;
    }

    test_socket.test_pos_write += len;
    return result;
}

static lmqtt_io_result_t read_buf(void *data, u8 *buf, int buf_len,
    int *bytes_read)
{
    test_buffer_t *source = (test_buffer_t *) data;

    return test_buffer_move(source, buf, &source->buf[source->pos], buf_len,
        bytes_read);
}

static lmqtt_io_result_t write_buf(void *data, u8 *buf, int buf_len,
    int *bytes_written)
{
    test_buffer_t *destination = (test_buffer_t *) data;

    return test_buffer_move(destination, &destination->buf[destination->pos],
        buf, buf_len, bytes_written);
}

static void on_connect(void *data)
{
    *((int *) data) = 1;
}

static void on_subscribe(void *data)
{
    *((int *) data) = 2;
}

static void on_unsubscribe(void *data)
{
    *((int *) data) = 3;
}

static lmqtt_connect_t connect;

static int do_connect_and_connack(lmqtt_client_t *client, long keep_alive,
    long timeout)
{
    lmqtt_client_initialize(client);
    lmqtt_client_set_default_timeout(client, timeout);
    test_socket_init_with_client(client);

    memset(&connect, 0, sizeof(connect));
    connect.keep_alive = keep_alive;
    connect.clean_session = 1;

    lmqtt_client_connect(client, &connect);

    test_socket_append(TEST_CONNACK_SUCCESS);

    return LMQTT_IO_STATUS_READY == client_process_output(client) &&
        LMQTT_IO_STATUS_READY == client_process_input(client) &&
        TEST_CONNECT == test_socket_shift();
}

static int do_connect(lmqtt_client_t *client, long keep_alive,
    long timeout)
{
    lmqtt_client_initialize(client);
    lmqtt_client_set_default_timeout(client, timeout);
    test_socket_init_with_client(client);

    memset(&connect, 0, sizeof(connect));
    connect.keep_alive = keep_alive;
    connect.clean_session = 1;

    lmqtt_client_connect(client, &connect);

    return LMQTT_IO_STATUS_READY == client_process_output(client) &&
        TEST_CONNECT == test_socket_shift();
}

START_TEST(should_initialize_client)
{
    lmqtt_client_t client;

    memset(&client, -1, sizeof(client));

    lmqtt_client_initialize(&client);

    ck_assert_ptr_eq(0, client.data);
    ck_assert(!client.read);
    ck_assert(!client.write);
    ck_assert_int_eq(0, client.failed);
    ck_assert_ptr_eq(&client.store, client.rx_state.store);
    ck_assert_ptr_eq(&client.store, client.tx_state.store);
}
END_TEST

START_TEST(should_prepare_connect_after_initialize)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;

    lmqtt_client_initialize(&client);
    test_socket_init_with_client(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;

    ck_assert_int_eq(1, lmqtt_client_connect(&client, &connect));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(TEST_CONNECT, test_socket_shift());
}
END_TEST

START_TEST(should_not_prepare_connect_twice)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;

    lmqtt_client_initialize(&client);
    test_socket_init_with_client(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;

    ck_assert_int_eq(1, lmqtt_client_connect(&client, &connect));
    client_process_output(&client);
    ck_assert_int_eq(TEST_CONNECT, test_socket_shift());

    ck_assert_int_eq(0, lmqtt_client_connect(&client, &connect));
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift());
}
END_TEST

START_TEST(should_not_prepare_invalid_connect)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;

    lmqtt_client_initialize(&client);
    test_socket_init_with_client(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 0; /* invalid; should have a non-empty client_id */

    ck_assert_int_eq(0, lmqtt_client_connect(&client, &connect));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(-1, test_socket_shift());
}
END_TEST

START_TEST(should_not_connect_with_full_store)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;
    int i;
    int res;

    lmqtt_client_initialize(&client);
    test_socket_init_with_client(&client);

    for (i = 0; lmqtt_store_append(&client.store, LMQTT_CLASS_PINGREQ, 0, 0); i++)
        ;

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;

    ck_assert_int_eq(0, lmqtt_client_connect(&client, &connect));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    do {
        int class;
        void *data;
        res = test_socket_shift();
        lmqtt_store_pop_any(&client.store, &class, &data);
    } while (res == TEST_PINGREQ);
    ck_assert_int_eq(-1, res);

    /* should work if trying again after store is empty */
    ck_assert_int_eq(1, lmqtt_client_connect(&client, &connect));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(TEST_CONNECT, test_socket_shift());
}
END_TEST

START_TEST(should_receive_connack_after_connect)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;
    int connected;

    lmqtt_client_initialize(&client);
    test_socket_init_with_client(&client);

    lmqtt_client_set_on_connect(&client, on_connect, &connected);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;
    lmqtt_client_connect(&client, &connect);
    client_process_output(&client);

    connected = 0;
    test_socket_append(TEST_CONNACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_input(&client));
    ck_assert_int_eq(1, connected);

    /* should not receive connack twice */
    connected = 0;
    test_socket_append(TEST_CONNACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_input(&client));
    ck_assert_int_eq(0, connected);
}
END_TEST

START_TEST(should_not_call_connect_callback_on_connect_failure)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;
    int connected;

    lmqtt_client_initialize(&client);
    test_socket_init_with_client(&client);

    lmqtt_client_set_on_connect(&client, on_connect, &connected);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;
    lmqtt_client_connect(&client, &connect);
    client_process_output(&client);

    connected = 0;
    test_socket_append(TEST_CONNACK_FAILURE);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_input(&client));
    ck_assert_int_eq(0, connected);
}
END_TEST

START_TEST(should_not_receive_connack_before_connect)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;
    int connected;

    lmqtt_client_initialize(&client);
    test_socket_init_with_client(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;
    lmqtt_client_set_on_connect(&client, on_connect, &connected);

    connected = 0;
    test_socket_append(TEST_CONNACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_input(&client));
    ck_assert_int_eq(0, connected);
}
END_TEST

START_TEST(should_subscribe)
{
    lmqtt_client_t client;
    lmqtt_subscribe_t subscribe;
    lmqtt_subscription_t subscription;
    int subscribed;

    ck_assert_int_eq(1, do_connect_and_connack(&client, 5, 3));

    lmqtt_client_set_on_subscribe(&client, on_subscribe, &subscribed);

    memset(&subscribe, 0, sizeof(subscribe));
    memset(&subscription, 0, sizeof(subscription));
    subscribe.count = 1;
    subscribe.subscriptions = &subscription;
    subscription.qos = 0;
    subscription.topic.buf = "test";
    subscription.topic.len = strlen(subscription.topic.buf);

    ck_assert_int_eq(1, lmqtt_client_subscribe(&client, &subscribe));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(TEST_SUBSCRIBE, test_socket_shift());

    subscribed = 0;
    test_socket_append(TEST_SUBACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_input(&client));
    ck_assert_int_eq(2, subscribed);
}
END_TEST

START_TEST(should_unsubscribe)
{
    lmqtt_client_t client;
    lmqtt_subscribe_t subscribe;
    lmqtt_subscription_t subscription;
    int unsubscribed;

    ck_assert_int_eq(1, do_connect_and_connack(&client, 5, 3));

    lmqtt_client_set_on_unsubscribe(&client, on_unsubscribe, &unsubscribed);

    memset(&subscribe, 0, sizeof(subscribe));
    memset(&subscription, 0, sizeof(subscription));
    subscribe.count = 1;
    subscribe.subscriptions = &subscription;
    subscription.topic.buf = "test";
    subscription.topic.len = strlen(subscription.topic.buf);

    ck_assert_int_eq(1, lmqtt_client_unsubscribe(&client, &subscribe));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(TEST_UNSUBSCRIBE, test_socket_shift());

    unsubscribed = 0;
    test_socket_append(TEST_UNSUBACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_input(&client));
    ck_assert_int_eq(3, unsubscribed);
}
END_TEST

START_TEST(should_assign_packet_ids_to_subscribe)
{
    lmqtt_client_t client;
    lmqtt_subscribe_t subscribe[3];
    lmqtt_subscription_t subscriptions[3];
    void *data;
    int i;

    memset(&subscribe, 0, sizeof(subscribe));
    memset(&subscriptions, 0, sizeof(subscriptions));
    for (i = 0; i < 3; i++) {
        subscribe[i].count = 1;
        subscribe[i].subscriptions = &subscriptions[i];
        subscriptions[i].topic.buf = "test";
        subscriptions[i].topic.len = strlen(subscriptions[i].topic.buf);
    }

    ck_assert_int_eq(1, do_connect_and_connack(&client, 5, 3));

    ck_assert_int_eq(1, lmqtt_client_subscribe(&client, &subscribe[0]));
    ck_assert_int_eq(1, lmqtt_client_subscribe(&client, &subscribe[1]));
    ck_assert_int_eq(1, lmqtt_client_subscribe(&client, &subscribe[2]));

    ck_assert_uint_eq(0, subscribe[0].packet_id);
    ck_assert_uint_eq(1, subscribe[1].packet_id);
    ck_assert_uint_eq(2, subscribe[2].packet_id);

    lmqtt_store_next(&client.store);
    lmqtt_store_next(&client.store);
    lmqtt_store_next(&client.store);

    ck_assert_int_eq(1, lmqtt_store_pop(&client.store, LMQTT_CLASS_SUBSCRIBE, 0, &data));
    ck_assert_int_eq(1, lmqtt_store_pop(&client.store, LMQTT_CLASS_SUBSCRIBE, 1, &data));
    ck_assert_int_eq(1, lmqtt_store_pop(&client.store, LMQTT_CLASS_SUBSCRIBE, 2, &data));
}
END_TEST

START_TEST(should_not_subscribe_with_invalid_packet)
{
    lmqtt_client_t client;
    lmqtt_subscribe_t subscribe;

    ck_assert_int_eq(1, do_connect_and_connack(&client, 5, 3));

    memset(&subscribe, 0, sizeof(subscribe));

    ck_assert_int_eq(0, lmqtt_client_subscribe(&client, &subscribe));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(-1, test_socket_shift());
}
END_TEST

START_TEST(should_not_subscribe_with_full_store)
{
    lmqtt_client_t client;
    lmqtt_subscribe_t subscribe;
    lmqtt_subscription_t subscription;
    int i;

    ck_assert_int_eq(1, do_connect_and_connack(&client, 5, 3));

    memset(&subscribe, 0, sizeof(subscribe));
    memset(&subscription, 0, sizeof(subscription));
    subscribe.count = 1;
    subscribe.subscriptions = &subscription;
    subscription.qos = 0;
    subscription.topic.buf = "test";
    subscription.topic.len = strlen(subscription.topic.buf);

    for (i = 0; lmqtt_store_append(&client.store, LMQTT_CLASS_PINGREQ, 0, 0); i++)
        ;

    ck_assert_int_eq(0, lmqtt_client_subscribe(&client, &subscribe));
}
END_TEST

START_TEST(should_send_pingreq_after_timeout)
{
    lmqtt_client_t client;
    long secs, nsecs;

    test_time_set(10, 0);
    ck_assert_int_eq(1, do_connect_and_connack(&client, 5, 3));

    test_time_set(15, 0);
    ck_assert_int_eq(1, lmqtt_client_get_timeout(&client, &secs, &nsecs));
    ck_assert_int_eq(0, secs);

    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(TEST_PINGREQ, test_socket_shift());

    ck_assert_int_eq(1, lmqtt_client_get_timeout(&client, &secs, &nsecs));
    ck_assert_int_eq(3, secs);
}
END_TEST

START_TEST(should_not_send_pingreq_before_timeout)
{
    lmqtt_client_t client;
    long secs, nsecs;

    test_time_set(10, 0);
    ck_assert_int_eq(1, do_connect_and_connack(&client, 5, 3));

    test_time_set(14, 0);
    ck_assert_int_eq(1, lmqtt_client_get_timeout(&client, &secs, &nsecs));
    ck_assert_int_eq(1, secs);

    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(-1, test_socket_shift());

    ck_assert_int_eq(1, lmqtt_client_get_timeout(&client, &secs, &nsecs));
    ck_assert_int_eq(1, secs);
}
END_TEST

START_TEST(should_not_send_pingreq_with_response_pending)
{
    lmqtt_client_t client;
    long secs, nsecs;

    test_time_set(10, 0);
    ck_assert_int_eq(1, do_connect_and_connack(&client, 5, 3));

    test_time_set(16, 0);
    client_keep_alive(&client);
    client_process_output(&client);
    ck_assert_int_eq(TEST_PINGREQ, test_socket_shift());

    test_time_set(17, 0);
    client_keep_alive(&client);
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift());
}
END_TEST

START_TEST(should_send_pingreq_after_pingresp)
{
    lmqtt_client_t client;
    long secs, nsecs;

    test_time_set(10, 0);
    ck_assert_int_eq(1, do_connect_and_connack(&client, 5, 3));

    test_time_set(16, 0);
    client_keep_alive(&client);
    client_process_output(&client);
    ck_assert_int_eq(TEST_PINGREQ, test_socket_shift());
    test_socket_append(TEST_PINGRESP);
    client_process_input(&client);

    test_time_set(22, 0);
    client_keep_alive(&client);
    client_process_output(&client);
    ck_assert_int_eq(TEST_PINGREQ, test_socket_shift());
}
END_TEST

START_TEST(should_fail_client_after_pingreq_timeout)
{
    lmqtt_client_t client;
    long secs, nsecs;

    test_time_set(10, 0);
    ck_assert_int_eq(1, do_connect_and_connack(&client, 5, 3));

    test_time_set(15, 0);
    ck_assert_int_eq(1, lmqtt_client_get_timeout(&client, &secs, &nsecs));
    ck_assert_int_eq(0, secs);

    client_keep_alive(&client);
    client_process_output(&client);
    ck_assert_int_eq(TEST_PINGREQ, test_socket_shift());

    test_time_set(20, 0);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_keep_alive(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_output(&client));
    ck_assert_int_eq(-1, test_socket_shift());
}
END_TEST

START_TEST(should_disconnect)
{
    lmqtt_client_t client;

    ck_assert_int_eq(1, do_connect_and_connack(&client, 5, 3));

    ck_assert_int_eq(1, lmqtt_client_disconnect(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_output(&client));
    ck_assert_int_eq(TEST_DISCONNECT, test_socket_shift());
}
END_TEST

START_TEST(should_not_send_pingreq_after_disconnect)
{
    lmqtt_client_t client;

    test_time_set(10, 0);
    do_connect_and_connack(&client, 5, 3);

    test_time_set(16, 0);
    ck_assert_int_eq(1, lmqtt_client_disconnect(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));

    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_output(&client));
    ck_assert_int_eq(TEST_DISCONNECT, test_socket_shift());
    ck_assert_int_eq(-1, test_socket_shift());
}
END_TEST

START_TEST(should_not_disconnect_before_connack)
{
    lmqtt_client_t client;

    do_connect(&client, 5, 3);

    ck_assert_int_eq(0, lmqtt_client_disconnect(&client));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_output(&client));
    ck_assert_int_eq(-1, test_socket_shift());
}
END_TEST

START_TEST(should_not_send_pingreq_after_failure)
{
    lmqtt_client_t client;

    test_time_set(10, 0);
    do_connect(&client, 5, 0);

    test_socket_append(TEST_CONNACK_FAILURE);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_input(&client));

    test_time_set(16, 0);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_keep_alive(&client));
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift());
}
END_TEST

START_TEST(should_not_send_pingreq_before_connack)
{
    lmqtt_client_t client;

    test_time_set(10, 0);
    do_connect(&client, 5, 0);

    test_time_set(16, 0);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift());
}
END_TEST

START_TEST(should_fail_client_after_connection_timeout)
{
    lmqtt_client_t client;

    test_time_set(10, 0);
    do_connect(&client, 5, 3);

    test_time_set(14, 0);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_keep_alive(&client));
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift());
}
END_TEST

START_TEST(should_not_fail_client_before_connection_timeout)
{
    lmqtt_client_t client;

    test_time_set(10, 0);
    do_connect(&client, 5, 7);

    test_time_set(16, 0);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift());
}
END_TEST

START_TEST(should_not_send_pingreq_with_zeroed_keep_alive)
{
    lmqtt_client_t client;

    test_time_set(10, 0);
    do_connect(&client, 0, 0);

    test_socket_append(TEST_CONNACK_SUCCESS);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_process_input(&client));

    test_time_set(11, 0);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));
    client_process_output(&client);
    ck_assert_int_eq(-1, test_socket_shift());
}
END_TEST

START_TCASE("Client commands")
{
    ADD_TEST(should_initialize_client);
    ADD_TEST(should_prepare_connect_after_initialize);
    ADD_TEST(should_not_prepare_connect_twice);
    ADD_TEST(should_not_prepare_invalid_connect);
    ADD_TEST(should_not_connect_with_full_store);
    ADD_TEST(should_receive_connack_after_connect);
    ADD_TEST(should_not_call_connect_callback_on_connect_failure);
    ADD_TEST(should_not_receive_connack_before_connect);

    ADD_TEST(should_subscribe);
    ADD_TEST(should_unsubscribe);
    ADD_TEST(should_assign_packet_ids_to_subscribe);
    ADD_TEST(should_not_subscribe_with_invalid_packet);
    ADD_TEST(should_not_subscribe_with_full_store);

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
}
END_TCASE
