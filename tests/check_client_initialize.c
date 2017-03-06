#include "check_lightmqtt.h"

#include "../src/lmqtt_io.c"

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

static struct {
    long secs;
    long nsecs;
} test_time;

static lmqtt_io_result_t get_time(long *secs, long *nsecs)
{
    *secs = test_time.secs;
    *nsecs = test_time.nsecs;
    return LMQTT_IO_SUCCESS;
}

static int prepare_connection(lmqtt_client_t *client, int timeout)
{
    test_buffer_t buf;
    lmqtt_connect_t connect;

    lmqtt_client_initialize(client);

    client->get_time = get_time;
    client->read = read_buf;
    client->write = write_buf;
    client->data = &buf;

    memset(&connect, 0, sizeof(connect));
    connect.keep_alive = timeout;

    memset(&buf, 0, sizeof(buf));
    buf.len = sizeof(buf.buf);
    buf.available_len = buf.len;

    lmqtt_client_connect(client, &connect);
    process_output(client);

    memcpy(buf.buf, "\x20\x02\x00\x00", 4);
    buf.pos = 0;
    buf.len = 4;
    buf.available_len = buf.len;

    return LMQTT_IO_STATUS_READY == process_input(client);
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
}
END_TEST

START_TEST(should_prepare_connect_after_initialize)
{
    test_buffer_t destination;
    lmqtt_client_t client;
    lmqtt_connect_t connect;

    lmqtt_client_initialize(&client);
    client.write = write_buf;
    client.data = &destination;

    memset(&connect, 0, sizeof(connect));
    memset(&destination, 0, sizeof(destination));
    destination.len = sizeof(destination.buf);
    destination.available_len = destination.len;

    ck_assert_int_eq(1, lmqtt_client_connect(&client, &connect));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, process_output(&client));

    ck_assert_int_eq(0x10, destination.buf[0]);
}
END_TEST

START_TEST(should_not_prepare_connect_twice)
{
    lmqtt_client_t client;
    lmqtt_connect_t connect;

    lmqtt_client_initialize(&client);

    memset(&connect, 0, sizeof(connect));

    ck_assert_int_eq(1, lmqtt_client_connect(&client, &connect));
    ck_assert_int_eq(0, lmqtt_client_connect(&client, &connect));
}
END_TEST

START_TEST(should_receive_connack_after_connect)
{
    test_buffer_t source;
    lmqtt_client_t client;
    lmqtt_connect_t connect;
    int connected = 0;

    lmqtt_client_initialize(&client);
    client.read = read_buf;
    client.data = &source;

    memset(&connect, 0, sizeof(connect));
    memset(&source, 0, sizeof(source));
    memcpy(source.buf, "\x20\x02", 2);
    source.len = 4;
    source.available_len = source.len;

    lmqtt_client_set_on_connect(&client, on_connect, &connected);

    ck_assert_int_eq(1, lmqtt_client_connect(&client, &connect));
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, process_input(&client));
    ck_assert_int_eq(1, connected);

    /* should not receive connack twice */
    source.pos = 0;
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, process_input(&client));
}
END_TEST

START_TEST(should_not_call_connect_callback_on_connect_failure)
{
    test_buffer_t source;
    lmqtt_client_t client;
    lmqtt_connect_t connect;
    int connected = 0;

    lmqtt_client_initialize(&client);
    client.read = read_buf;
    client.data = &source;

    memset(&connect, 0, sizeof(connect));
    memset(&source, 0, sizeof(source));
    memcpy(source.buf, "\x20\x02\x00\x01", 4);
    source.len = 4;
    source.available_len = source.len;

    lmqtt_client_set_on_connect(&client, on_connect, &connected);

    ck_assert_int_eq(1, lmqtt_client_connect(&client, &connect));
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, process_input(&client));
    ck_assert_int_eq(0, connected);
}
END_TEST

START_TEST(should_not_receive_connack_before_connect)
{
    test_buffer_t source;
    lmqtt_client_t client;
    lmqtt_connect_t connect;
    int connected = 0;

    lmqtt_client_initialize(&client);
    client.read = read_buf;
    client.data = &source;

    memset(&connect, 0, sizeof(connect));
    memset(&source, 0, sizeof(source));
    memcpy(source.buf, "\x20\x02", 2);
    source.len = 4;
    source.available_len = source.len;

    lmqtt_client_set_on_connect(&client, on_connect, &connected);

    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, process_input(&client));
    ck_assert_int_eq(0, connected);
}
END_TEST

START_TEST(should_send_pingreq_after_timeout)
{
    test_buffer_t destination;
    lmqtt_client_t client;

    test_time.secs = 10;
    test_time.nsecs = 0;

    ck_assert_int_eq(1, prepare_connection(&client, 5));

    test_time.secs = 16;
    test_time.nsecs = 0;

    ck_assert_int_eq(LMQTT_IO_STATUS_READY, client_keep_alive(&client));

    memset(&destination, 0, sizeof(destination));
    destination.len = sizeof(destination.buf);
    destination.available_len = destination.len;

    client.write = write_buf;
    client.data = &destination;

    ck_assert_int_eq(LMQTT_IO_STATUS_READY, process_output(&client));

    ck_assert_int_eq(2, destination.pos);
    ck_assert_int_eq(0xc0, destination.buf[0]);
}
END_TEST

START_TCASE("Client initialize")
{
    ADD_TEST(should_initialize_client);
    ADD_TEST(should_prepare_connect_after_initialize);
    ADD_TEST(should_not_prepare_connect_twice);
    ADD_TEST(should_receive_connack_after_connect);
    ADD_TEST(should_not_call_connect_callback_on_connect_failure);
    ADD_TEST(should_not_receive_connack_before_connect);
    ADD_TEST(should_send_pingreq_after_timeout);
}
END_TCASE
