#include "check_lightmqtt.h"

#include "lightmqtt/packet.h"

static lmqtt_io_result_t move_buf(test_buffer_t *test_buffer, u8 *dst, u8 *src,
    int len, int *bytes_written)
{
    int cnt = test_buffer->available_len - test_buffer->pos;
    if (cnt > len)
        cnt = len;
    memcpy(dst, src, cnt);
    *bytes_written = cnt;
    test_buffer->pos += cnt;
    test_buffer->call_count += 1;
    return cnt == 0 && test_buffer->available_len < test_buffer->len ?
        LMQTT_IO_AGAIN : LMQTT_IO_SUCCESS;
}

#include "../src/lmqtt_io.c"

static test_buffer_t test_source;
static test_buffer_t test_destination;

static lmqtt_io_result_t read_test_buf(void *data, u8 *buf, int buf_len,
    int *bytes_read)
{
    test_buffer_t *source = (test_buffer_t *) data;

    return move_buf(source, buf, &source->buf[source->pos], buf_len,
        bytes_read);
}

static lmqtt_io_result_t write_test_buf(void *data, u8 *buf, int buf_len,
    int *bytes_written)
{
    test_buffer_t *destination = (test_buffer_t *) data;

    return move_buf(destination, &destination->buf[destination->pos], buf,
        buf_len, bytes_written);
}

lmqtt_io_result_t lmqtt_rx_buffer_decode(lmqtt_rx_buffer_t *state, u8 *buf,
    int buf_len, int *bytes_read)
{
    return move_buf(&test_destination,
        &test_destination.buf[test_destination.pos], buf, buf_len, bytes_read);
}

lmqtt_io_result_t lmqtt_tx_buffer_encode(lmqtt_tx_buffer_t *state, u8 *buf,
    int buf_len, int *bytes_written)
{
    return move_buf(&test_source, buf, &test_source.buf[test_source.pos],
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
    test_buffer_t source;
    lmqtt_io_status_t res;

    memset(&client, 0, sizeof(client));
    memset(&source, 0, sizeof(source));
    memset(&test_destination, 0, sizeof(test_destination));

    client.data = &source;
    client.read = read_test_buf;
    test_destination.len = sizeof(test_destination.buf);
    test_destination.available_len = test_destination.len;

    res = process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, res);

    ck_assert_int_eq(0, source.pos);
    ck_assert_int_eq(1, source.call_count);
    ck_assert_int_eq(0, test_destination.pos);
    ck_assert_int_eq(0, test_destination.call_count);
}
END_TEST

START_TEST(should_process_input_with_complete_read_and_complete_process)
{
    lmqtt_client_t client;
    test_buffer_t source;
    lmqtt_io_status_t res;

    memset(&client, 0, sizeof(client));
    memset(&source, 0, sizeof(source));
    memset(&test_destination, 0, sizeof(test_destination));

    client.data = &source;
    client.read = read_test_buf;
    test_destination.len = sizeof(test_destination.buf);
    test_destination.available_len = test_destination.len;
    memset(&source.buf, 0xf, 5);
    source.len = 5;
    source.available_len = source.len;

    res = process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, res);

    ck_assert_int_eq(5, source.pos);
    ck_assert_int_eq(2, source.call_count);
    ck_assert_int_eq(5, test_destination.pos);
    ck_assert_int_eq(1, test_destination.call_count);
}
END_TEST

START_TEST(should_consume_read_buffer_if_write_interrupts)
{
    lmqtt_client_t client;
    test_buffer_t source;
    int i;
    lmqtt_io_status_t res;

    memset(&client, 0, sizeof(client));
    memset(&source, 0, sizeof(source));
    memset(&test_destination, 0, sizeof(test_destination));

    client.data = &source;
    client.read = read_test_buf;
    test_destination.len = sizeof(test_destination.buf);
    test_destination.available_len = LMQTT_RX_BUFFER_SIZE / 2;
    source.len = LMQTT_RX_BUFFER_SIZE / 4 * 5;
    source.available_len = source.len;
    for (i = 0; i < LMQTT_RX_BUFFER_SIZE / 4 * 5; i++)
        source.buf[i] = i % 199;

    res = process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, res);

    /* will process half of a buffer; should read whole input (5/4 of a buffer),
       and leave 3/4 of a buffer to process next time */
    ck_assert_int_eq(LMQTT_RX_BUFFER_SIZE / 4 * 5, source.pos);
    ck_assert_int_eq(3, source.call_count);
    ck_assert_int_eq(LMQTT_RX_BUFFER_SIZE / 2, test_destination.pos);
    ck_assert_int_eq(2, test_destination.call_count);
    ck_assert_uint_eq(0 % 199, test_destination.buf[0]);
    ck_assert_uint_eq((LMQTT_RX_BUFFER_SIZE / 2 - 1) % 199,
        test_destination.buf[LMQTT_RX_BUFFER_SIZE / 2 - 1]);
}
END_TEST

START_TEST(should_fill_read_buffer_if_write_interrupts)
{
    lmqtt_client_t client;
    test_buffer_t source;
    int i;
    lmqtt_io_status_t res;

    memset(&client, 0, sizeof(client));
    memset(&source, 0, sizeof(source));
    memset(&test_destination, 0, sizeof(test_destination));

    client.data = &source;
    client.read = read_test_buf;
    test_destination.len = sizeof(test_destination.buf);
    test_destination.available_len = LMQTT_RX_BUFFER_SIZE / 2;
    source.len = sizeof(source.buf);
    source.available_len = source.len;
    for (i = 0; i < sizeof(source.buf); i++)
        source.buf[i] = i % 199;

    res = process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, res);

    /* will process half of a buffer; should read at most one and a half
       buffer */
    ck_assert_int_eq(LMQTT_RX_BUFFER_SIZE / 2 * 3, source.pos);
    ck_assert_int_eq(2, source.call_count);
    ck_assert_int_eq(LMQTT_RX_BUFFER_SIZE / 2, test_destination.pos);
    ck_assert_int_eq(2, test_destination.call_count);
    ck_assert_uint_eq(0 % 199, test_destination.buf[0]);
    ck_assert_uint_eq((LMQTT_RX_BUFFER_SIZE / 2 - 1) % 199,
        test_destination.buf[LMQTT_RX_BUFFER_SIZE / 2 - 1]);
}
END_TEST

START_TEST(should_process_remaining_input_from_previous_call)
{
    lmqtt_client_t client;
    test_buffer_t source;
    int i;
    static int s = LMQTT_RX_BUFFER_SIZE / 8;
    lmqtt_io_status_t res;

    memset(&client, 0, sizeof(client));
    memset(&source, 0, sizeof(source));
    memset(&test_destination, 0, sizeof(test_destination));

    client.data = &source;
    client.read = read_test_buf;
    test_destination.len = sizeof(test_destination.buf);
    test_destination.available_len = s;
    source.len = sizeof(source.buf);
    source.available_len = source.len;
    for (i = 0; i < sizeof(source.buf); i++)
        source.buf[i] = i % 199;

    res = process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, res);

    ck_assert_int_eq(s, test_destination.pos);
    ck_assert_int_eq(2, test_destination.call_count);
    ck_assert_uint_eq(0 % 199, test_destination.buf[0]);
    ck_assert_uint_eq((s - 1) % 199, test_destination.buf[s - 1]);

    test_destination.available_len += s;
    res = process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_DATA, res);

    ck_assert_int_eq(s + s, test_destination.pos);
    ck_assert_int_eq(4, test_destination.call_count);
    ck_assert_uint_eq(s % 199, test_destination.buf[s]);
    ck_assert_uint_eq((s + s - 1) % 199, test_destination.buf[s + s - 1]);
}
END_TEST

START_TEST(should_decode_remaining_buffer_if_read_blocks)
{
    lmqtt_client_t client;
    test_buffer_t source;
    lmqtt_io_status_t res;

    memset(&client, 0, sizeof(client));
    memset(&source, 0, sizeof(source));
    memset(&test_destination, 0, sizeof(test_destination));

    client.data = &source;
    client.read = read_test_buf;
    test_destination.len = sizeof(test_destination.buf);
    test_destination.available_len = test_destination.len;
    memset(&source.buf, 0xf, 5);
    source.len = 5;
    source.available_len = 2;

    res = process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, res);

    ck_assert_int_eq(2, source.pos);
    ck_assert_int_eq(2, test_destination.pos);
}
END_TEST

START_TEST(should_not_decode_remaining_buffer_if_read_fails)
{
    lmqtt_client_t client;
    test_buffer_t source;
    lmqtt_io_status_t res;

    memset(&client, 0, sizeof(client));
    memset(&source, 0, sizeof(source));
    memset(&test_destination, 0, sizeof(test_destination));

    client.data = &source;
    client.read = read_test_buf_fail;
    test_destination.len = sizeof(test_destination.buf);
    test_destination.available_len = test_destination.len;
    source.len = sizeof(source.buf);
    source.available_len = source.len;

    res = process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, res);

    ck_assert_int_eq(1, source.call_count);
    ck_assert_int_eq(0, test_destination.call_count);

    res = process_input(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_ERROR, res);

    /* should not try any other I/O after failure */
    ck_assert_int_eq(1, source.call_count);
    ck_assert_int_eq(0, test_destination.call_count);
}
END_TEST

START_TEST(should_process_output_with_complete_encode_and_complete_write)
{
    lmqtt_client_t client;
    test_buffer_t destination;
    lmqtt_io_status_t res;

    memset(&client, 0, sizeof(client));
    memset(&destination, 0, sizeof(destination));
    memset(&test_source, 0, sizeof(test_source));

    client.data = &destination;
    client.write = write_test_buf;
    destination.len = sizeof(destination.buf);
    destination.available_len = destination.len;
    memset(&test_source.buf, 0xf, 5);
    test_source.len = 5;
    test_source.available_len = test_source.len;

    res = process_output(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_READY, res);

    ck_assert_int_eq(5, destination.pos);
    ck_assert_int_eq(1, destination.call_count);
    ck_assert_int_eq(5, test_source.pos);
    ck_assert_int_eq(2, test_source.call_count);
}
END_TEST

START_TEST(should_encode_remaining_buffer_if_write_blocks)
{
    lmqtt_client_t client;
    test_buffer_t destination;
    lmqtt_io_status_t res;

    memset(&client, 0, sizeof(client));
    memset(&destination, 0, sizeof(destination));
    memset(&test_source, 0, sizeof(test_source));

    client.data = &destination;
    client.write = write_test_buf;
    destination.len = sizeof(destination.buf);
    destination.available_len = 2;
    memset(&test_source.buf, 0xf, 20);
    test_source.len = 20;
    test_source.available_len = test_source.len;

    res = process_output(&client);
    ck_assert_int_eq(LMQTT_IO_STATUS_BLOCK_CONN, res);

    ck_assert_int_eq(2, destination.pos);
    ck_assert_int_eq(20, test_source.pos);
}
END_TEST

START_TCASE("Process")
{
    ADD_TEST(should_process_input_without_data);
    ADD_TEST(should_consume_read_buffer_if_write_interrupts);
    ADD_TEST(should_fill_read_buffer_if_write_interrupts);
    ADD_TEST(should_process_remaining_input_from_previous_call);
    ADD_TEST(should_decode_remaining_buffer_if_read_blocks);
    ADD_TEST(should_not_decode_remaining_buffer_if_read_fails);
    ADD_TEST(should_process_output_with_complete_encode_and_complete_write);
    ADD_TEST(should_encode_remaining_buffer_if_write_blocks);
}
END_TCASE
