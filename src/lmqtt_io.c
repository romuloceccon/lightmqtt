#include <lightmqtt/io.h>
#include <string.h>

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
    int read_allowed = 1;
    int decode_allowed = 1;

    while (read_allowed || decode_allowed) {
        int bytes_read;

        read_allowed = read_allowed && client->read_buf_pos <
            sizeof(client->read_buf);

        if (read_allowed) {
            client->read(client->data,
                &client->read_buf[client->read_buf_pos],
                sizeof(client->read_buf) - client->read_buf_pos, &bytes_read);
            client->read_buf_pos += bytes_read;
            if (bytes_read == 0)
                read_allowed = 0;
        }

        decode_allowed = decode_allowed && client->read_buf_pos > 0;

        if (decode_allowed) {
            lmqtt_rx_buffer_decode(&client->rx_state, client->read_buf,
                client->read_buf_pos, &bytes_read);
            memmove(&client->read_buf[0], &client->read_buf[bytes_read],
                client->read_buf_pos - bytes_read);
            client->read_buf_pos -= bytes_read;
            if (bytes_read == 0)
                decode_allowed = 0;
        }
    }
}

/*
 * TODO: review how lmqtt_tx_buffer_encode() should handle cases where some read
 * would block, cases where there's no data to encode and cases where the buffer
 * is not enough to encode the whole command.
 */
static void process_output(lmqtt_client_t *client)
{
    int encode_allowed = 1;
    int write_allowed = 1;

    while (encode_allowed || write_allowed) {
        int bytes_written;

        encode_allowed = encode_allowed && client->write_buf_pos <
            sizeof(client->write_buf);

        if (encode_allowed) {
            lmqtt_tx_buffer_encode(&client->tx_state,
                &client->write_buf[client->write_buf_pos],
                sizeof(client->write_buf) - client->write_buf_pos,
                &bytes_written);
            client->write_buf_pos += bytes_written;
            if (bytes_written == 0)
                encode_allowed = 0;
        }

        write_allowed = write_allowed && client->write_buf_pos > 0;

        if (write_allowed) {
            client->write(client->data, client->write_buf,
                client->write_buf_pos, &bytes_written);
            memmove(&client->write_buf[0], &client->write_buf[bytes_written],
                client->write_buf_pos - bytes_written);
            client->write_buf_pos -= bytes_written;
            if (bytes_written == 0)
                write_allowed = 0;
        }
    }
}
