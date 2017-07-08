#include "check_lightmqtt.h"

#define RX_BUFFER_SIZE 512
#define TX_BUFFER_SIZE 512

static test_socket_t ts;
static char topic[4096];
static test_buffer_t payload;
static lmqtt_store_entry_t entries[16];
static unsigned char rx_buffer[RX_BUFFER_SIZE];
static unsigned char tx_buffer[TX_BUFFER_SIZE];

static lmqtt_io_result_t test_read_blocked(void *data, void *buf,
    size_t buf_len, size_t *bytes_read, int *os_error)
{
    *bytes_read = 0;
    return LMQTT_IO_WOULD_BLOCK;
}

static lmqtt_io_result_t test_read_fail(void *data, void *buf, size_t buf_len,
    size_t *bytes_read, int *os_error)
{
    *bytes_read = 0;
    return LMQTT_IO_ERROR;
}

static void on_connect(void *data, lmqtt_connect_t *connect, int succeeded)
{
    int *connected = (int *) data;
    *connected = (succeeded && connect->response.return_code == 0);
}

static void do_client_initialize(lmqtt_client_t *client)
{
    lmqtt_client_callbacks_t callbacks;
    lmqtt_client_buffers_t buffers;

    test_socket_init(&ts);
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
}

static int on_message_received(void *data, lmqtt_publish_t *publish)
{
    return 1;
}

static lmqtt_allocate_result_t on_publish_allocate_topic(void *data,
    lmqtt_publish_t *publish, size_t len)
{
    publish->topic.len = len;
    publish->topic.buf = topic;
    return LMQTT_ALLOCATE_SUCCESS;
}

static lmqtt_allocate_result_t on_publish_allocate_payload_block(void *data,
    lmqtt_publish_t *publish, size_t len)
{
    publish->payload.len = len;
    publish->payload.data = &payload;
    publish->payload.write = &test_buffer_write;
    return LMQTT_ALLOCATE_SUCCESS;
}

START_TEST(should_run_before_connect)
{
    lmqtt_client_t client;
    lmqtt_string_t *str_rd, *str_wr;
    int res;

    do_client_initialize(&client);

    res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert(!LMQTT_IS_ERROR(res));
    ck_assert(LMQTT_IS_EOF(res)); /* encoder is closed; act like at eof */
    ck_assert(!LMQTT_IS_EOF_RD(res));
    ck_assert(LMQTT_IS_EOF_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_WR(res));
    ck_assert(!LMQTT_IS_QUEUEABLE(res));
    ck_assert_int_eq(0, LMQTT_ERROR_NUM(res));
}
END_TEST

START_TEST(should_run_after_connect)
{
    lmqtt_string_t dummy;
    lmqtt_client_t client;
    lmqtt_string_t *str_rd = &dummy, *str_wr = &dummy;
    lmqtt_connect_t connect;
    int res;
    int connected = -1;

    do_client_initialize(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;

    lmqtt_client_set_on_connect(&client, on_connect, &connected);
    lmqtt_client_connect(&client, &connect);
    test_socket_append(&ts, TEST_CONNACK_SUCCESS);

    res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert_int_eq(TEST_CONNECT, test_socket_shift(&ts));
    ck_assert_int_eq(1, connected);

    ck_assert(!LMQTT_IS_ERROR(res));
    ck_assert(!LMQTT_IS_EOF(res));
    ck_assert(!LMQTT_IS_EOF_RD(res));
    ck_assert(!LMQTT_IS_EOF_WR(res));
    ck_assert(LMQTT_WOULD_BLOCK_CONN_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_WR(res));
    ck_assert(LMQTT_IS_QUEUEABLE(res));
    ck_assert_int_eq(0, LMQTT_ERROR_NUM(res));

    ck_assert_ptr_eq(NULL, str_rd);
    ck_assert_ptr_eq(NULL, str_wr);
}
END_TEST

START_TEST(should_run_with_output_blocked)
{
    lmqtt_string_t dummy;
    lmqtt_client_t client;
    lmqtt_string_t *str_rd = &dummy, *str_wr = &dummy;
    lmqtt_connect_t connect;
    int res;

    do_client_initialize(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;

    lmqtt_client_connect(&client, &connect);
    ts.write_buf.available_len = 2;

    res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert_int_eq(-2, test_socket_shift(&ts));

    ck_assert(!LMQTT_IS_ERROR(res));
    ck_assert(!LMQTT_IS_EOF(res));
    ck_assert(!LMQTT_IS_EOF_RD(res));
    ck_assert(!LMQTT_IS_EOF_WR(res));
    ck_assert(LMQTT_WOULD_BLOCK_CONN_RD(res));
    ck_assert(LMQTT_WOULD_BLOCK_CONN_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_WR(res));
    ck_assert(LMQTT_IS_QUEUEABLE(res));
    ck_assert_int_eq(0, LMQTT_ERROR_NUM(res));

    ck_assert_ptr_eq(NULL, str_rd);
    ck_assert_ptr_eq(NULL, str_wr);
}
END_TEST

START_TEST(should_run_with_data_blocked_for_read)
{
    lmqtt_string_t dummy;
    lmqtt_client_t client;
    lmqtt_string_t *str_rd = &dummy, *str_wr = &dummy;
    lmqtt_connect_t connect;
    int res;

    do_client_initialize(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;
    connect.client_id.len = 10;
    connect.client_id.read = test_read_blocked;

    lmqtt_client_connect(&client, &connect);

    res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert(!LMQTT_IS_ERROR(res));
    ck_assert(!LMQTT_IS_EOF(res));
    ck_assert(!LMQTT_IS_EOF_RD(res));
    ck_assert(!LMQTT_IS_EOF_WR(res));
    ck_assert(LMQTT_WOULD_BLOCK_CONN_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_WR(res));
    ck_assert(LMQTT_WOULD_BLOCK_DATA_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_WR(res));
    ck_assert(LMQTT_IS_QUEUEABLE(res));
    ck_assert_int_eq(0, LMQTT_ERROR_NUM(res));

    ck_assert_ptr_eq(&connect.client_id, str_rd);
    ck_assert_ptr_eq(NULL, str_wr);
}
END_TEST

START_TEST(should_run_with_data_blocked_for_write)
{
    lmqtt_string_t dummy;
    lmqtt_client_t client;
    lmqtt_string_t *str_rd = &dummy, *str_wr = &dummy;
    lmqtt_connect_t connect;
    lmqtt_message_callbacks_t message_callbacks;
    int res;

    do_client_initialize(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;
    lmqtt_client_connect(&client, &connect);
    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    lmqtt_client_run_once(&client, &str_rd, &str_wr);

    memset(&message_callbacks, 0, sizeof(message_callbacks));
    message_callbacks.on_publish = &on_message_received;
    message_callbacks.on_publish_allocate_topic = &on_publish_allocate_topic;
    message_callbacks.on_publish_allocate_payload =
        &on_publish_allocate_payload_block;
    lmqtt_client_set_message_callbacks(&client, &message_callbacks);

    test_socket_append(&ts, TEST_PUBLISH_QOS_0_BIG);
    payload.len = sizeof(payload.buf);
    payload.available_len = 1;
    res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert(!LMQTT_IS_ERROR(res));
    ck_assert(!LMQTT_IS_EOF(res));
    ck_assert(!LMQTT_IS_EOF_RD(res));
    ck_assert(!LMQTT_IS_EOF_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_RD(res));
    ck_assert(LMQTT_WOULD_BLOCK_DATA_WR(res));
    ck_assert(LMQTT_IS_QUEUEABLE(res));
    ck_assert_int_eq(0, LMQTT_ERROR_NUM(res));

    ck_assert_ptr_eq(NULL, str_rd);
    ck_assert_ptr_eq(&client.rx_state.internal.publish.payload, str_wr);
}
END_TEST

START_TEST(should_run_with_read_error)
{
    lmqtt_client_t client;
    lmqtt_string_t *str_rd, *str_wr;
    lmqtt_connect_t connect;
    int res;

    do_client_initialize(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;
    connect.client_id.len = 10;
    connect.client_id.read = test_read_fail;

    lmqtt_client_connect(&client, &connect);

    res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert(LMQTT_IS_ERROR(res));
    ck_assert(!LMQTT_IS_EOF(res));
    ck_assert(!LMQTT_IS_EOF_RD(res));
    ck_assert(!LMQTT_IS_EOF_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_WR(res));
    ck_assert(!LMQTT_IS_QUEUEABLE(res));
    ck_assert_int_eq(0, LMQTT_ERROR_NUM(res));
}
END_TEST

START_TEST(should_run_with_output_closed)
{
    lmqtt_client_t client;
    lmqtt_string_t *str_rd, *str_wr;
    lmqtt_connect_t connect;
    int res;

    do_client_initialize(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;

    lmqtt_client_connect(&client, &connect);
    test_socket_append(&ts, TEST_CONNACK_SUCCESS);

    ts.write_buf.len = 0;
    ts.write_buf.available_len = 0;

    res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert(!LMQTT_IS_ERROR(res));
    ck_assert(LMQTT_IS_EOF(res));
    ck_assert(!LMQTT_IS_EOF_RD(res));
    ck_assert(LMQTT_IS_EOF_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_WR(res));
    ck_assert(!LMQTT_IS_QUEUEABLE(res));
    ck_assert_int_eq(0, LMQTT_ERROR_NUM(res));
}
END_TEST

START_TEST(should_run_with_input_closed)
{
    lmqtt_client_t client;
    lmqtt_string_t *str_rd, *str_wr;
    lmqtt_connect_t connect;
    int res;

    do_client_initialize(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;

    lmqtt_client_connect(&client, &connect);

    ts.read_buf.len = 0;
    ts.read_buf.available_len = 0;

    res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert(!LMQTT_IS_ERROR(res));
    ck_assert(LMQTT_IS_EOF(res));
    ck_assert(LMQTT_IS_EOF_RD(res));
    ck_assert(!LMQTT_IS_EOF_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_WR(res));
    ck_assert(!LMQTT_IS_QUEUEABLE(res));
    ck_assert_int_eq(0, LMQTT_ERROR_NUM(res));
}
END_TEST

START_TEST(should_run_with_queue_full)
{
    lmqtt_client_t client;
    lmqtt_string_t *str_rd, *str_wr;
    lmqtt_connect_t connect;
    lmqtt_publish_t publish;
    char message[TX_BUFFER_SIZE * 2];
    int res;

    do_client_initialize(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;

    lmqtt_client_connect(&client, &connect);
    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    lmqtt_client_run_once(&client, &str_rd, &str_wr);

    memset(message, 'x', sizeof(message));
    memset(&publish, 0, sizeof(publish));
    publish.qos = LMQTT_QOS_1;
    publish.topic.buf = "topic";
    publish.topic.len = strlen(publish.topic.buf);
    publish.payload.buf = message;
    publish.payload.len = sizeof(message);

    while (lmqtt_client_publish(&client, &publish))
        ;

    ts.write_buf.available_len = ts.write_buf.pos;

    res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert(!LMQTT_IS_ERROR(res));
    ck_assert(!LMQTT_IS_EOF(res));
    ck_assert(!LMQTT_IS_EOF_RD(res));
    ck_assert(!LMQTT_IS_EOF_WR(res));
    ck_assert(LMQTT_WOULD_BLOCK_CONN_RD(res));
    ck_assert(LMQTT_WOULD_BLOCK_CONN_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_WR(res));
    ck_assert(!LMQTT_IS_QUEUEABLE(res));
    ck_assert_int_eq(0, LMQTT_ERROR_NUM(res));
}
END_TEST

START_TEST(should_run_with_existing_session)
{
    lmqtt_client_t client;
    lmqtt_string_t *str_rd, *str_wr;
    lmqtt_connect_t connect;
    lmqtt_publish_t publish;
    lmqtt_store_value_t value;
    int res;

    do_client_initialize(&client);

    memset(&publish, 0, sizeof(publish));
    publish.qos = LMQTT_QOS_1;
    publish.topic.buf = "topic";
    publish.topic.len = strlen(publish.topic.buf);
    publish.payload.buf = "payload";
    publish.payload.len = strlen(publish.payload.buf);
    value.value = &publish;
    value.callback = NULL;
    value.callback_data = &client;
    lmqtt_store_append(&client.main_store, LMQTT_KIND_PUBLISH_1, &value);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 0;
    connect.client_id.buf = "test";
    connect.client_id.len = 4;

    lmqtt_client_connect(&client, &connect);
    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert(!LMQTT_IS_ERROR(res));
    ck_assert(!LMQTT_IS_EOF(res));
    ck_assert(!LMQTT_IS_EOF_RD(res));
    ck_assert(!LMQTT_IS_EOF_WR(res));
    ck_assert(LMQTT_WOULD_BLOCK_CONN_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_WR(res));
    ck_assert(LMQTT_IS_QUEUEABLE(res));
    ck_assert_int_eq(0, LMQTT_ERROR_NUM(res));

    ck_assert_int_eq(TEST_CONNECT, test_socket_shift(&ts));
    ck_assert_int_eq(TEST_PUBLISH, test_socket_shift(&ts));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_run_with_keep_alive)
{
    lmqtt_client_t client;
    lmqtt_string_t *str_rd, *str_wr;
    lmqtt_connect_t connect;
    int res;

    do_client_initialize(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;
    connect.keep_alive = 10;

    test_time_set(5, 0);
    lmqtt_client_connect(&client, &connect);
    test_socket_append(&ts, TEST_CONNACK_SUCCESS);
    lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert_int_eq(TEST_CONNECT, test_socket_shift(&ts));

    test_time_set(16, 0);
    res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert(!LMQTT_IS_ERROR(res));
    ck_assert(!LMQTT_IS_EOF(res));
    ck_assert(!LMQTT_IS_EOF_RD(res));
    ck_assert(!LMQTT_IS_EOF_WR(res));
    ck_assert(LMQTT_WOULD_BLOCK_CONN_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_WR(res));
    ck_assert(LMQTT_IS_QUEUEABLE(res));
    ck_assert_int_eq(0, LMQTT_ERROR_NUM(res));

    ck_assert_int_eq(TEST_PINGREQ, test_socket_shift(&ts));
    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TEST(should_run_after_timeout)
{
    lmqtt_client_t client;
    lmqtt_string_t *str_rd, *str_wr;
    lmqtt_connect_t connect;
    int res;

    do_client_initialize(&client);

    memset(&connect, 0, sizeof(connect));
    connect.clean_session = 1;

    test_time_set(5, 0);
    lmqtt_client_set_default_timeout(&client, 10);
    lmqtt_client_connect(&client, &connect);
    lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert_int_eq(TEST_CONNECT, test_socket_shift(&ts));

    test_time_set(16, 0);
    res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

    ck_assert(LMQTT_IS_ERROR(res));
    ck_assert(!LMQTT_IS_EOF(res));
    ck_assert(!LMQTT_IS_EOF_RD(res));
    ck_assert(!LMQTT_IS_EOF_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_CONN_WR(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_RD(res));
    ck_assert(!LMQTT_WOULD_BLOCK_DATA_WR(res));
    ck_assert(!LMQTT_IS_QUEUEABLE(res));
    ck_assert_int_eq(0, LMQTT_ERROR_NUM(res));

    ck_assert_int_eq(-1, test_socket_shift(&ts));
}
END_TEST

START_TCASE("Client run once")
{
    ADD_TEST(should_run_before_connect);
    ADD_TEST(should_run_after_connect);
    ADD_TEST(should_run_with_output_blocked);
    ADD_TEST(should_run_with_data_blocked_for_read);
    ADD_TEST(should_run_with_data_blocked_for_write);
    ADD_TEST(should_run_with_read_error);
    ADD_TEST(should_run_with_output_closed);
    ADD_TEST(should_run_with_input_closed);
    ADD_TEST(should_run_with_queue_full);
    ADD_TEST(should_run_with_existing_session);
    ADD_TEST(should_run_with_keep_alive);
    ADD_TEST(should_run_after_timeout);
}
END_TCASE
