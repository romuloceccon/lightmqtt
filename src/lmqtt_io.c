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

static int buffer_read(lmqtt_read_t reader, void *data, u8 *buf, int *buf_pos,
    int buf_len)
{
    int bytes_read;

    reader(data, &buf[*buf_pos], buf_len - *buf_pos, &bytes_read);
    *buf_pos += bytes_read;

    return bytes_read > 0;
}

static int buffer_write(lmqtt_write_t writer, void *data, u8 *buf, int *buf_pos)
{
    int bytes_written;

    writer(data, buf, *buf_pos, &bytes_written);
    memmove(&buf[0], &buf[bytes_written], *buf_pos - bytes_written);
    *buf_pos -= bytes_written;

    return bytes_written > 0;
}

static void buffer_transfer(lmqtt_read_t reader, void *reader_data,
    lmqtt_write_t writer, void *writer_data, u8 *buf, int *buf_pos, int buf_len)
{
    int read_allowed = 1;
    int write_allowed = 1;

    while (read_allowed || write_allowed) {
        read_allowed = read_allowed && *buf_pos < buf_len &&
            buffer_read(reader, reader_data, buf, buf_pos, buf_len);

        write_allowed = write_allowed && *buf_pos > 0 &&
            buffer_write(writer, writer_data, buf, buf_pos);
    }
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
static void process_input(lmqtt_client_t *client)
{
    buffer_transfer(
        client->read, client->data,
        (lmqtt_write_t) lmqtt_rx_buffer_decode, &client->rx_state,
        client->read_buf, &client->read_buf_pos, sizeof(client->read_buf));
}

/*
 * TODO: review how lmqtt_tx_buffer_encode() should handle cases where some read
 * would block, cases where there's no data to encode and cases where the buffer
 * is not enough to encode the whole command.
 */
static void process_output(lmqtt_client_t *client)
{
    buffer_transfer(
        (lmqtt_read_t) lmqtt_tx_buffer_encode, &client->tx_state,
        client->write, client->data,
        client->write_buf, &client->write_buf_pos, sizeof(client->write_buf));
}
