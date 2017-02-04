/*
 * TODO: somehow we should tell the user which handle she should select() on. If
 * (a) process_rx_buffer() returns LMQTT_DECODE_AGAIN (meaning a write operation
 * would block), (b) the read handle is still readable and (c) the buffer is
 * full, the user cannot select() on the read handle. Otherwise the program will
 * enter a busy loop because the read handle remains signaled, but
 * process_input() cannot consume the buffer, which would take the read handle
 * out of its signaled state. Similarly, one should not wait on the write handle
 * which some callback is writing the input message to if the input buffer is
 * empty.
 */
static void process_input(LMqttClient *client)
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
            res = process_rx_buffer(0, client->read_buf, client->read_buf_pos,
                &bytes_read);
            memmove(&client->read_buf[0], &client->read_buf[bytes_read],
                client->read_buf_pos - bytes_read);
            client->read_buf_pos -= bytes_read;
            if (res == LMQTT_DECODE_AGAIN)
                process_allowed = 0;
        }
    }
}
