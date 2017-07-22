#include "check_lightmqtt.h"

#define BYTE_AT(p) ((p) % 199 + 1)

#define RX_BUFFER_SIZE 512
#define TX_BUFFER_SIZE 512
#define ENTRY_COUNT 1

#define CHECK_BUF_FILL_AT(test_buf, n) \
    ck_assert_uint_eq(BYTE_AT(n), test_buf[n])

#define CHECK_BUF_ZERO_AT(test_buf, n) ck_assert_uint_eq(0, test_buf[n])

#define RX_4TH (RX_BUFFER_SIZE / 4)

static int buffer_fail;
static lmqtt_error_t error_decode;
static lmqtt_error_t error_encode;
static int error_os_decode;
static int error_os_encode;
static lmqtt_client_t client;
static test_buffer_t test_src;
static test_buffer_t test_dst;
static unsigned char rx_buffer[RX_BUFFER_SIZE];
static unsigned char tx_buffer[TX_BUFFER_SIZE];
static lmqtt_store_entry_t entries[ENTRY_COUNT];

lmqtt_io_result_t lmqtt_rx_buffer_decode_mock(lmqtt_rx_buffer_t *state,
    unsigned char *buf, size_t buf_len, size_t *bytes_read)
{
    if (buffer_fail)
        return LMQTT_IO_ERROR;

    return test_buffer_move(&test_dst,
        &test_dst.buf[test_dst.pos], buf, buf_len, bytes_read);
}

lmqtt_error_t lmqtt_rx_buffer_get_error_mock(lmqtt_rx_buffer_t *state,
    int *os_error)
{
    ck_assert_ptr_eq(&client.rx_state, state);
    *os_error = error_os_decode;
    return error_decode;
}

lmqtt_io_result_t lmqtt_tx_buffer_encode_mock(lmqtt_tx_buffer_t *state,
    unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    if (buffer_fail)
        return LMQTT_IO_ERROR;

    return test_buffer_move(&test_src, buf, &test_src.buf[test_src.pos],
        buf_len, bytes_written);
}

lmqtt_error_t lmqtt_tx_buffer_get_error_mock(lmqtt_tx_buffer_t *state,
    int *os_error)
{
    ck_assert_ptr_eq(&client.tx_state, state);
    *os_error = error_os_encode;
    return error_encode;
}

static void prepare_all()
{
    int i;

    memset(&client, 0, sizeof(client));
    memset(&test_src, 0, sizeof(test_src));
    memset(&test_dst, 0, sizeof(test_dst));
    memset(entries, 0, sizeof(entries));

    error_decode = 0;
    error_encode = 0;
    error_os_decode = 0;
    error_os_encode = 0;
    buffer_fail = 0;

    client.connect_store.get_time = &test_time_get;
    client.connect_store.entries = &client.connect_store_entry;
    client.connect_store.capacity = 1;
    client.main_store.get_time = &test_time_get;
    client.main_store.entries = entries;
    client.main_store.capacity = ENTRY_COUNT;
    client.current_store = &client.main_store;

    test_src.len = sizeof(test_src.buf);
    test_dst.len = sizeof(test_dst.buf);

    for (i = 0; i < test_src.len; i++)
        test_src.buf[i] = BYTE_AT(i);
}

static void prepare_read()
{
    prepare_all();

    client.callbacks.data = &test_src;
    client.callbacks.read = test_buffer_read;
    client.read_buf = rx_buffer;
    client.read_buf_capacity = RX_BUFFER_SIZE;
}

static void prepare_write()
{
    prepare_all();

    client.callbacks.data = &test_dst;
    client.callbacks.write = test_buffer_write;
    client.write_buf = tx_buffer;
    client.write_buf_capacity = TX_BUFFER_SIZE;
}

START_TEST(should_process_input_without_data)
{
    lmqtt_io_status_t res;

    prepare_read();

    test_src.len = 0;
    test_src.available_len = 0;
    test_dst.available_len = test_dst.len;

    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, res);

    ck_assert_int_eq(0, test_src.pos);
    ck_assert_int_eq(1, test_src.call_count);
    ck_assert_int_eq(0, test_dst.pos);
    ck_assert_int_eq(0, test_dst.call_count);
}
END_TEST

START_TEST(should_process_input_with_complete_read_and_complete_decode)
{
    lmqtt_io_status_t res;

    prepare_read();

    test_src.len = 5;
    test_src.available_len = test_src.len;
    test_dst.available_len = test_dst.len;

    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, res);

    ck_assert_int_eq(5, test_src.pos);
    ck_assert_int_eq(2, test_src.call_count);
    ck_assert_int_eq(5, test_dst.pos);
    ck_assert_int_eq(1, test_dst.call_count);
}
END_TEST

START_TEST(should_consume_read_buffer_after_decode_blocks)
{
    lmqtt_io_status_t res;

    prepare_read();

    test_src.available_len = 5 * RX_4TH;
    test_dst.available_len = 2 * RX_4TH;

    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, res);

    /*
     * Each position in the drawing below represents LMQTT_RX_BUFFER_SIZE / 8
     * bytes. The flow of bytes between the buffers/streams is shown in 4 steps.
     *
     * read buf / test_src (stream, open ended):
     *   **: bytes which can be read without blocking
     *   ..: bytes read
     *
     * rx buf (fixed buf):
     *   **: bytes used
     *     : free space
     *
     * decoded / test_dst (stream, open ended):
     *   ..: bytes which could be written without blocking
     *   **: bytes written
     *
     *     read buf         rx buf           decoded
     * 1. |**********      |        |        |....      # initial
     * 2. |**........      |********|        |....      # transf from test_src
     * 3. |**........      |****    |        |****      # transf to test_dst
     * 4. |..........      |******  |        |****      # consume test_src
     */
    ck_assert_int_eq(5 * RX_4TH, test_src.pos);
    ck_assert_int_eq(3, test_src.call_count);

    ck_assert_int_eq(2 * RX_4TH, test_dst.pos);
    ck_assert_int_eq(2, test_dst.call_count);

    CHECK_BUF_FILL_AT(test_dst.buf,              0);
    CHECK_BUF_FILL_AT(test_dst.buf, 2 * RX_4TH - 1);
    CHECK_BUF_ZERO_AT(test_dst.buf,     2 * RX_4TH);
}
END_TEST

START_TEST(should_fill_read_buffer_if_decode_interrupts)
{
    lmqtt_io_status_t res;

    prepare_read();

    test_src.available_len = test_src.len;
    test_dst.available_len = 2 * RX_4TH;

    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, res);

    /*
     *     read buf           rx buf            decoded
     * 1. |****************  |        |        |....     # initial
     * 2. |********........  |********|        |....     # transf from test_src
     * 3. |********........  |****    |        |****     # transf to test_dst
     * 4. |****............  |********|        |****     # fill rx buf
     */
    ck_assert_int_eq(6 * RX_4TH, test_src.pos);
    ck_assert_int_eq(2, test_src.call_count);

    ck_assert_int_eq(2 * RX_4TH, test_dst.pos);
    ck_assert_int_eq(2, test_dst.call_count);

    CHECK_BUF_FILL_AT(test_dst.buf,              0);
    CHECK_BUF_FILL_AT(test_dst.buf, 2 * RX_4TH - 1);
    CHECK_BUF_ZERO_AT(test_dst.buf,     2 * RX_4TH);
}
END_TEST

START_TEST(should_process_remaining_input_from_previous_call)
{
    lmqtt_io_status_t res;

    prepare_read();

    test_src.available_len = test_src.len;
    test_dst.available_len = RX_4TH / 2;

    /*
     *     read buf           rx buf            decoded
     * 1. |****************  |        |        |.         # initial
     * 2. |********........  |********|        |.         # transf from test_src
     * 3. |********........  |******* |        |*         # transf to test_dst
     * 4. |*******.........  |********|        |*         # fill rx buf
     */
    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, res);

    ck_assert_int_eq(RX_4TH / 2, test_dst.pos);
    ck_assert_int_eq(2, test_dst.call_count);

    CHECK_BUF_FILL_AT(test_dst.buf,              0);
    CHECK_BUF_FILL_AT(test_dst.buf, RX_4TH / 2 - 1);
    CHECK_BUF_ZERO_AT(test_dst.buf,     RX_4TH / 2);

    test_dst.available_len += RX_4TH / 2;

    /*
     * More room available in test_dst:
     *
     *     read buf           rx buf            decoded
     * 1. |*******.........  |********|        |*.       # increase test_dst buf
     * 2. |*******.........  |******* |        |**       # transf to test_dst
     * 3. |******..........  |********|        |**       # fill rx buf
     */
    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, res);

    ck_assert_int_eq(RX_4TH, test_dst.pos);
    ck_assert_int_eq(4, test_dst.call_count);

    CHECK_BUF_FILL_AT(test_dst.buf, RX_4TH / 2);
    CHECK_BUF_FILL_AT(test_dst.buf, RX_4TH - 1);
    CHECK_BUF_ZERO_AT(test_dst.buf,     RX_4TH);
}
END_TEST

START_TEST(should_decode_remaining_buffer_if_read_blocks)
{
    lmqtt_io_status_t res;

    prepare_read();

    test_src.len = 5;
    test_src.available_len = 2;
    test_dst.available_len = test_dst.len;

    /*
     * (Each char here is a single byte)
     *
     *     read buf        rx buf            decoded
     * 1. |**             |           |     |..........   # initial
     * 2. |..             |**         |     |..........   # transf from test_src
     * 3. |..             |           |     |**........   # transf to test_dst
     */
    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, res);

    ck_assert_int_eq(2, test_src.pos);
    ck_assert_int_eq(2, test_dst.pos);
}
END_TEST

START_TEST(should_return_block_data_if_both_read_and_decode_block)
{
    lmqtt_io_status_t res;

    prepare_read();

    test_src.available_len = 4;
    test_dst.available_len = 2;

    /*
     * (Each char here is a single byte)
     *
     *     read buf        rx buf            decoded
     * 1. |****           |           |     |..           # initial
     * 2. |....           |****       |     |..           # transf from test_src
     * 3. |....           |**         |     |**           # transf to test_dst
     */
    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, res);

    ck_assert_int_eq(4, test_src.pos);
    ck_assert_int_eq(2, test_dst.pos);
}
END_TEST

START_TEST(should_not_decode_remaining_buffer_if_read_fails)
{
    lmqtt_io_status_t res;

    prepare_read();

    client.callbacks.read = &test_buffer_io_fail;
    test_src.available_len = test_src.len;
    test_dst.available_len = test_dst.len;

    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, res);

    ck_assert_int_eq(1, test_src.call_count);
    ck_assert_int_eq(0, test_dst.call_count);

    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, res);

    /* should not try any other I/O after failure */
    ck_assert_int_eq(1, test_src.call_count);
    ck_assert_int_eq(0, test_dst.call_count);
}
END_TEST

START_TEST(should_encode_remaining_buffer_if_write_blocks)
{
    lmqtt_io_status_t res;

    prepare_write();

    test_src.available_len = 20;
    test_dst.available_len = 2;

    res = client_process_output(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, res);

    ck_assert_int_eq(2, test_dst.pos);
    ck_assert_int_eq(20, test_src.pos);
}
END_TEST

START_TEST(should_write_remaining_buffer_if_encode_reaches_eof)
{
    lmqtt_io_status_t res;

    prepare_write();

    test_src.len = 5;
    test_src.available_len = test_src.len;
    test_dst.available_len = test_dst.len;

    res = client_process_output(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, res);

    ck_assert_int_eq(5, test_dst.pos);
    ck_assert_int_eq(1, test_dst.call_count);
    ck_assert_int_eq(5, test_src.pos);
    ck_assert_int_eq(2, test_src.call_count);
}
END_TEST

START_TEST(should_return_block_conn_if_both_encode_and_write_block)
{
    lmqtt_io_status_t res;

    prepare_write();

    test_src.available_len = 4;
    test_dst.available_len = 2;

    res = client_process_output(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, res);

    ck_assert_int_eq(4, test_src.pos);
    ck_assert_int_eq(2, test_dst.pos);
}
END_TEST

START_TEST(should_return_eof_if_encode_blocks_and_write_closes)
{
    lmqtt_io_status_t res;

    prepare_write();

    test_src.available_len = 5 * RX_4TH;
    test_dst.len = 2 * RX_4TH;
    test_dst.available_len = test_dst.len;

    res = client_process_output(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, res);

    ck_assert_int_eq(5 * RX_4TH, test_src.pos);
    ck_assert_int_eq(2 * RX_4TH, test_dst.pos);
}
END_TEST

START_TEST(should_touch_store_after_read)
{
    prepare_read();

    test_src.len = 5;
    test_src.available_len = test_src.len;
    test_dst.available_len = test_dst.len;

    test_time_set(5, 0);
    client_process_input(&client);

    ck_assert_int_eq(5, client.main_store.last_touch.secs);
}
END_TEST

START_TEST(should_not_touch_store_if_read_blocks)
{
    prepare_read();

    test_src.len = 5;
    test_src.available_len = 1;
    test_dst.available_len = test_dst.len;

    test_time_set(5, 0);
    client_process_input(&client);

    test_time_set(8, 0);
    client_process_input(&client);

    ck_assert_int_eq(5, client.main_store.last_touch.secs);
}
END_TEST

START_TEST(should_touch_store_when_decode_unblocks)
{
    prepare_read();

    test_src.len = 5;
    test_src.available_len = 3;
    test_dst.available_len = 0;

    test_time_set(5, 0);
    client_process_input(&client);

    test_dst.available_len = test_dst.len;
    test_time_set(8, 0);
    client_process_input(&client);

    ck_assert_int_eq(8, client.main_store.last_touch.secs);
}
END_TEST

START_TEST(should_not_set_error_if_decode_does_not_fail)
{
    prepare_read();

    test_src.available_len = 5;
    error_decode = LMQTT_ERROR_DECODE_PUBLISH_TOPIC_WRITE_FAILED;
    error_os_decode = 123;

    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, client_process_input(&client));
    ck_assert_int_eq(0, client.error);
    ck_assert_int_eq(0, client.os_error);
}
END_TEST

START_TEST(should_set_error_if_read_fails)
{
    prepare_read();

    test_dst.available_len = 5;
    client.callbacks.read = &test_buffer_io_fail;

    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_input(&client));
    ck_assert_int_eq(LMQTT_ERROR_CONNECTION_READ, client.error);
    ck_assert_int_eq(1, client.os_error);
}
END_TEST

START_TEST(should_set_error_if_write_fails)
{
    prepare_write();

    test_src.available_len = 5;
    client.callbacks.write = &test_buffer_io_fail;

    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_output(&client));
    ck_assert_int_eq(LMQTT_ERROR_CONNECTION_WRITE, client.error);
    ck_assert_int_eq(1, client.os_error);
}
END_TEST

START_TEST(should_set_error_if_decode_fails)
{
    prepare_read();

    test_src.available_len = 5;
    buffer_fail = 1;
    error_decode = LMQTT_ERROR_DECODE_PUBLISH_TOPIC_WRITE_FAILED;
    error_os_decode = 123;

    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_input(&client));
    ck_assert_int_eq(LMQTT_ERROR_DECODE_PUBLISH_TOPIC_WRITE_FAILED,
        client.error);
    ck_assert_int_eq(123, client.os_error);
}
END_TEST

START_TEST(should_set_error_if_encode_fails)
{
    prepare_write();

    test_dst.available_len = 5;
    buffer_fail = 1;
    error_encode = LMQTT_ERROR_ENCODE_STRING;
    error_os_encode = 456;

    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, client_process_output(&client));
    ck_assert_int_eq(LMQTT_ERROR_ENCODE_STRING, client.error);
    ck_assert_int_eq(456, client.os_error);
}
END_TEST

START_TCASE("Client buffers")
{
    lmqtt_rx_buffer_decode = &lmqtt_rx_buffer_decode_mock;
    lmqtt_rx_buffer_get_error = &lmqtt_rx_buffer_get_error_mock;
    lmqtt_tx_buffer_encode = &lmqtt_tx_buffer_encode_mock;
    lmqtt_tx_buffer_get_error = &lmqtt_tx_buffer_get_error_mock;

    ADD_TEST(should_process_input_without_data);
    ADD_TEST(should_process_input_with_complete_read_and_complete_decode);
    ADD_TEST(should_consume_read_buffer_after_decode_blocks);
    ADD_TEST(should_fill_read_buffer_if_decode_interrupts);
    ADD_TEST(should_process_remaining_input_from_previous_call);
    ADD_TEST(should_decode_remaining_buffer_if_read_blocks);
    ADD_TEST(should_return_block_data_if_both_read_and_decode_block);
    ADD_TEST(should_not_decode_remaining_buffer_if_read_fails);
    ADD_TEST(should_encode_remaining_buffer_if_write_blocks);
    ADD_TEST(should_write_remaining_buffer_if_encode_reaches_eof);
    ADD_TEST(should_return_block_conn_if_both_encode_and_write_block);
    ADD_TEST(should_return_eof_if_encode_blocks_and_write_closes);
    ADD_TEST(should_touch_store_after_read);
    ADD_TEST(should_not_touch_store_if_read_blocks);
    ADD_TEST(should_touch_store_when_decode_unblocks);

    ADD_TEST(should_not_set_error_if_decode_does_not_fail);
    ADD_TEST(should_set_error_if_read_fails);
    ADD_TEST(should_set_error_if_write_fails);
    ADD_TEST(should_set_error_if_decode_fails);
    ADD_TEST(should_set_error_if_encode_fails);
}
END_TCASE
