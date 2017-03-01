#include <lightmqtt/io.h>
#include <string.h>

static lmqtt_io_result_t decode_wrapper(void *data, u8 *buf, int buf_len,
    int *bytes_read)
{
    return lmqtt_rx_buffer_decode((lmqtt_rx_buffer_t *) data, buf, buf_len,
        bytes_read);
}

static lmqtt_io_result_t encode_wrapper(void *data, u8 *buf, int buf_len,
    int *bytes_written)
{
    return lmqtt_tx_buffer_encode((lmqtt_tx_buffer_t *) data, buf, buf_len,
        bytes_written);
}

static lmqtt_io_result_t buffer_read(lmqtt_read_t reader, void *data, u8 *buf,
    int *buf_pos, int buf_len, int *cnt)
{
    lmqtt_io_result_t result;

    result = reader(data, &buf[*buf_pos], buf_len - *buf_pos, cnt);
    *buf_pos += *cnt;

    return result;
}

static lmqtt_io_result_t buffer_write(lmqtt_write_t writer, void *data, u8 *buf,
    int *buf_pos, int *cnt)
{
    lmqtt_io_result_t result;

    result = writer(data, buf, *buf_pos, cnt);
    memmove(&buf[0], &buf[*cnt], *buf_pos - *cnt);
    *buf_pos -= *cnt;

    return result;
}

static int buffer_check(lmqtt_io_result_t io_res, int *cnt,
    lmqtt_io_status_t block_status, lmqtt_io_status_t *transfer_result,
    int *failed)
{
    if (io_res == LMQTT_IO_AGAIN) {
        *transfer_result = block_status;
    } else if (io_res == LMQTT_IO_ERROR) {
        *transfer_result = LMQTT_IO_STATUS_ERROR;
        *failed = 1;
    }
    return *cnt > 0;
}

/*
 * TODO: test what happens if both reader and writer block. Behavior should be
 * consistent between input and output, i.e., either LMQTT_IO_STATUS_BLOCK_CONN
 * or LMQTT_IO_STATUS_BLOCK_DATA should be prefered.
 */
static lmqtt_io_status_t buffer_transfer(lmqtt_read_t reader, void *reader_data,
    lmqtt_io_status_t reader_block, lmqtt_write_t writer, void *writer_data,
    lmqtt_io_status_t writer_block, u8 *buf, int *buf_pos, int buf_len,
    int *failed)
{
    int read_allowed = 1;
    int write_allowed = 1;
    lmqtt_io_status_t result = LMQTT_IO_STATUS_READY;

    if (*failed)
        return LMQTT_IO_STATUS_ERROR;

    while (read_allowed || write_allowed) {
        int cnt;

        read_allowed = read_allowed && !*failed && *buf_pos < buf_len &&
            buffer_check(
                buffer_read(reader, reader_data, buf, buf_pos, buf_len, &cnt),
                &cnt, reader_block, &result, failed);

        write_allowed = write_allowed && !*failed && *buf_pos > 0 &&
            buffer_check(
                buffer_write(writer, writer_data, buf, buf_pos, &cnt),
                &cnt, writer_block, &result, failed);
    }

    return result;
}

/*
 * TODO: somehow we should tell the user which handle she should select() on. If
 * (a) lmqtt_rx_buffer_decode() returns LMQTT_DECODE_CONTINUE (meaning a write operation
 * would block), (b) the read handle is still readable and (c) the buffer is
 * full, the user cannot select() on the read handle. Otherwise the program will
 * enter a busy loop because the read handle remains signaled, but
 * process_input() cannot consume the buffer, which would take the read handle
 * out of its signaled state. Similarly, one should not wait on the write handle
 * which some callback is writing the input message to if the input buffer is
 * empty.
 */
lmqtt_io_status_t process_input(lmqtt_client_t *client)
{
    return buffer_transfer(
        client->read, client->data, LMQTT_IO_STATUS_BLOCK_CONN,
        (lmqtt_write_t) lmqtt_rx_buffer_decode, &client->rx_state,
            LMQTT_IO_STATUS_BLOCK_DATA,
        client->read_buf, &client->read_buf_pos, sizeof(client->read_buf),
        &client->failed);
}

/*
 * TODO: review how lmqtt_tx_buffer_encode() should handle cases where some read
 * would block, cases where there's no data to encode and cases where the buffer
 * is not enough to encode the whole command.
 */
lmqtt_io_status_t process_output(lmqtt_client_t *client)
{
    return buffer_transfer(
        (lmqtt_read_t) lmqtt_tx_buffer_encode, &client->tx_state,
            LMQTT_IO_STATUS_BLOCK_DATA,
        client->write, client->data, LMQTT_IO_STATUS_BLOCK_CONN,
        client->write_buf, &client->write_buf_pos, sizeof(client->write_buf),
        &client->failed);
}
