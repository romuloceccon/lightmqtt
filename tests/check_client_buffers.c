#include "check_lightmqtt.h"

#include "lightmqtt/packet.h"

#define BYTE_AT(p) ((p) % 199 + 1)

#define PREPARE_READ \
    do { \
        int i; \
        memset(&client, 0, sizeof(client)); \
        memset(&test_src, 0, sizeof(test_src)); \
        memset(&test_dst, 0, sizeof(test_dst)); \
        client.data = &test_src; \
        client.read = test_buffer_read; \
        test_src.len = sizeof(test_src.buf); \
        test_dst.len = sizeof(test_dst.buf); \
        for (i = 0; i < test_src.len; i++) \
            test_src.buf[i] = BYTE_AT(i); \
    } while (0)

#define PREPARE_WRITE \
    do { \
        int i; \
        memset(&client, 0, sizeof(client)); \
        memset(&test_dst, 0, sizeof(test_dst)); \
        memset(&test_src, 0, sizeof(test_src)); \
        client.data = &test_dst; \
        client.write = test_buffer_write; \
        test_src.len = sizeof(test_src.buf); \
        test_dst.len = sizeof(test_dst.buf); \
        for (i = 0; i < test_src.len; i++) \
            test_src.buf[i] = BYTE_AT(i); \
    } while (0)

#define CHECK_BUF_FILL_AT(test_buf, n) \
    ck_assert_uint_eq(BYTE_AT(n), test_buf[n])

#define CHECK_BUF_ZERO_AT(test_buf, n) ck_assert_uint_eq(0, test_buf[n])

#define RX_4TH (LMQTT_RX_BUFFER_SIZE / 4)

#include "../src/lmqtt_io.c"

static test_buffer_t test_src;
static test_buffer_t test_dst;

lmqtt_io_result_t lmqtt_rx_buffer_decode(lmqtt_rx_buffer_t *state, u8 *buf,
    int buf_len, int *bytes_read)
{
    return test_buffer_move(&test_dst,
        &test_dst.buf[test_dst.pos], buf, buf_len, bytes_read);
}

lmqtt_io_result_t lmqtt_tx_buffer_encode(lmqtt_tx_buffer_t *state, u8 *buf,
    int buf_len, int *bytes_written)
{
    return test_buffer_move(&test_src, buf, &test_src.buf[test_src.pos],
        buf_len, bytes_written);
}

static lmqtt_io_result_t read_test_buf_fail(void *data, u8 *buf, int buf_len,
    int *bytes_read)
{
    test_buffer_t *source = (test_buffer_t *) data;
    source->call_count += 1;
    return LMQTT_IO_ERROR;
}

START_TEST(should_process_input_without_data)
{
    lmqtt_client_t client;
    lmqtt_io_status_t res;

    PREPARE_READ;

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
    lmqtt_client_t client;
    lmqtt_io_status_t res;

    PREPARE_READ;

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
    lmqtt_client_t client;
    lmqtt_io_status_t res;

    PREPARE_READ;

    test_src.len = 5 * RX_4TH;
    test_src.available_len = test_src.len;
    test_dst.available_len = 2 * RX_4TH;

    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, res);

    /*
     * Each position in the drawing below represents LMQTT_RX_BUFFER_SIZE / 8
     * bytes. The flow of bytes between the buffers/streams is shown in 4 steps.
     *
     * read buf (stream, open ended):
     *   **: bytes which can be read without blocking
     *   ++: bytes not yet available (reading would block)
     *
     * rx buf (fixed buf):
     *   **: bytes used
     *     : free space
     *
     * decoded (stream, open ended):
     *   ..: bytes which could be written without blocking
     *     : bytes not yet writable (writing would block)
     *
     *     read buf         rx buf           decoded
     * 1. |**********      |        |        |....
     * 2. |**              |********|        |....
     * 3. |**              |****    |        |****
     * 4. |                |******  |        |****
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
    lmqtt_client_t client;
    lmqtt_io_status_t res;

    PREPARE_READ;

    test_src.available_len = test_src.len;
    test_dst.available_len = 2 * RX_4TH;

    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, res);

    /*
     *     read buf           rx buf            decoded
     * 1. |****************  |        |        |....
     * 2. |********          |********|        |....
     * 3. |********          |****    |        |****
     * 4. |****              |********|        |****
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
    lmqtt_client_t client;
    lmqtt_io_status_t res;

    PREPARE_READ;

    test_src.available_len = test_src.len;
    test_dst.available_len = RX_4TH / 2;

    /*
     *     read buf           rx buf            decoded
     * 1. |****************  |        |        |.
     * 2. |********          |********|        |.
     * 3. |********          |******* |        |*
     * 4. |*******           |********|        |*
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
     *     read buf           rx buf            decoded
     * 1. |*******           |********|        |*.
     * 2. |*******           |******* |        |**
     * 3. |******            |********|        |**
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
    lmqtt_client_t client;
    lmqtt_io_status_t res;

    PREPARE_READ;

    test_src.len = 5;
    test_src.available_len = 2;
    test_dst.available_len = test_dst.len;

    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, res);

    ck_assert_int_eq(2, test_src.pos);
    ck_assert_int_eq(2, test_dst.pos);
}
END_TEST

START_TEST(should_return_block_conn_if_both_read_and_decode_block)
{
    lmqtt_client_t client;
    lmqtt_io_status_t res;

    PREPARE_READ;

    test_src.available_len = 4;
    test_dst.available_len = 2;

    res = client_process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, res);

    ck_assert_int_eq(4, test_src.pos);
    ck_assert_int_eq(2, test_dst.pos);
}
END_TEST

START_TEST(should_not_decode_remaining_buffer_if_read_fails)
{
    lmqtt_client_t client;
    lmqtt_io_status_t res;

    PREPARE_READ;

    client.read = read_test_buf_fail;
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

START_TEST(should_process_output_with_complete_encode_and_complete_write)
{
    lmqtt_client_t client;
    lmqtt_io_status_t res;

    PREPARE_WRITE;

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

START_TEST(should_encode_remaining_buffer_if_write_blocks)
{
    lmqtt_client_t client;
    lmqtt_io_status_t res;

    PREPARE_WRITE;

    test_src.len = 20;
    test_src.available_len = test_src.len;
    test_dst.available_len = 2;

    res = client_process_output(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, res);

    ck_assert_int_eq(2, test_dst.pos);
    ck_assert_int_eq(20, test_src.pos);
}
END_TEST

START_TEST(should_return_block_conn_if_both_encode_and_write_block)
{
    lmqtt_client_t client;
    lmqtt_io_status_t res;

    PREPARE_WRITE;

    test_src.available_len = 4;
    test_dst.available_len = 2;

    res = client_process_output(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, res);

    ck_assert_int_eq(4, test_src.pos);
    ck_assert_int_eq(2, test_dst.pos);
}
END_TEST

START_TCASE("Client buffers")
{
    ADD_TEST(should_process_input_without_data);
    ADD_TEST(should_process_input_with_complete_read_and_complete_decode);
    ADD_TEST(should_consume_read_buffer_after_decode_blocks);
    ADD_TEST(should_fill_read_buffer_if_decode_interrupts);
    ADD_TEST(should_process_remaining_input_from_previous_call);
    ADD_TEST(should_decode_remaining_buffer_if_read_blocks);
    ADD_TEST(should_return_block_conn_if_both_read_and_decode_block);
    ADD_TEST(should_not_decode_remaining_buffer_if_read_fails);
    ADD_TEST(should_process_output_with_complete_encode_and_complete_write);
    ADD_TEST(should_encode_remaining_buffer_if_write_blocks);
    ADD_TEST(should_return_block_conn_if_both_encode_and_write_block);
}
END_TCASE
