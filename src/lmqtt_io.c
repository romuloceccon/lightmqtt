#include <lightmqtt/io.h>
#include <string.h>

/*
 * TODO: somehow we should tell the user which handle she should select() on. If
 * (a) decode_rx_buffer() returns LMQTT_DECODE_AGAIN (meaning a write operation
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
    int read_allowed = 1;
    int process_allowed = 1;

    while (read_allowed && client->read_buf_pos < sizeof(client->read_buf) ||
        process_allowed && client->read_buf_pos > 0) {
        int res;
        int bytes_read;

        if (read_allowed && client->read_buf_pos < sizeof(client->read_buf)) {
            res = client->read(client->data,
                &client->read_buf[client->read_buf_pos],
                sizeof(client->read_buf) - client->read_buf_pos, &bytes_read);
            client->read_buf_pos += bytes_read;
            if (res == LMQTT_ERR_AGAIN)
                read_allowed = 0;
        }

        if (process_allowed && client->read_buf_pos > 0) {
            res = decode_rx_buffer(&client->rx_state, client->read_buf,
                client->read_buf_pos, &bytes_read);
            memmove(&client->read_buf[0], &client->read_buf[bytes_read],
                client->read_buf_pos - bytes_read);
            client->read_buf_pos -= bytes_read;
            if (res == LMQTT_DECODE_AGAIN)
                process_allowed = 0;
        }
    }
}

/*
 * TODO: review how encode_tx_buffer() should handle cases where some read
 * would block, cases where there's no data to encode and cases where the buffer
 * is not enough to encode the whole command.
 */
static void process_output(lmqtt_client_t *client)
{
    int build_allowed = 1;
    int write_allowed = 1;

    while (build_allowed && client->write_buf_pos < sizeof(client->write_buf) ||
        write_allowed && client->write_buf_pos > 0) {
        int res;
        int bytes_written;

        if (build_allowed && client->write_buf_pos < sizeof(client->write_buf)) {
            res = encode_tx_buffer(&client->tx_state,
                &client->write_buf[client->write_buf_pos],
                sizeof(client->write_buf) - client->write_buf_pos,
                &bytes_written);
            client->write_buf_pos += bytes_written;
            if (res == LMQTT_ENCODE_AGAIN)
                build_allowed = 0;
        }

        if (write_allowed && client->write_buf_pos > 0) {
            res = client->write(client->data, client->write_buf,
                client->write_buf_pos, &bytes_written);
            memmove(&client->write_buf[0], &client->write_buf[bytes_written],
                client->write_buf_pos - bytes_written);
            client->write_buf_pos -= bytes_written;
            if (res == LMQTT_ERR_AGAIN)
                write_allowed = 0;
        }
    }
}
