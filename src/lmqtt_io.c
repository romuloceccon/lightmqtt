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
        /* if both input and output block prefer LMQTT_IO_STATUS_BLOCK_CONN */
        if (*transfer_result != LMQTT_IO_STATUS_BLOCK_CONN)
            *transfer_result = block_status;
        return 0;
    }

    if (io_res == LMQTT_IO_ERROR || *failed) {
        *transfer_result = LMQTT_IO_STATUS_ERROR;
        *failed = 1;
        return 0;
    }

    return *cnt > 0;
}

static lmqtt_io_status_t buffer_transfer(
    lmqtt_read_t reader, void *reader_data, lmqtt_io_status_t reader_block,
    lmqtt_write_t writer, void *writer_data, lmqtt_io_status_t writer_block,
    u8 *buf, int *buf_pos, int buf_len, int *failed)
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

/******************************************************************************
 * lmqtt_client_t PRIVATE functions
 ******************************************************************************/

static int client_on_connack_fail(void *data, lmqtt_connack_t *connack);
static int client_on_connack(void *data, lmqtt_connack_t *connack);
static int client_on_pingresp_fail(void *data);
static int client_on_pingresp(void *data);
static int client_do_connect_fail(lmqtt_client_t *client,
    lmqtt_connect_t *connect);
static int client_do_connect(lmqtt_client_t *client, lmqtt_connect_t *connect);
static int client_do_pingreq_fail(lmqtt_client_t *client);
static int client_do_pingreq(lmqtt_client_t *client);

static int client_on_connack_fail(void *data, lmqtt_connack_t *connack)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    client->failed = 1;

    return 1;
}

static int client_on_connack(void *data, lmqtt_connack_t *connack)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    client->internal.rx_callbacks.on_connack = client_on_connack_fail;

    if (connack->return_code == LMQTT_CONNACK_RC_ACCEPTED) {
        client->internal.pingreq = client_do_pingreq;
        client->internal.rx_callbacks.on_pingresp = client_on_pingresp;

        if (client->on_connect)
            client->on_connect(client->on_connect_data);
    } else {
        client->failed = 1;
    }

    return 1;
}

static int client_on_pingresp_fail(void *data)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    client->failed = 1;

    return 1;
}

static int client_on_pingresp(void *data)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    return 1;
}

static int client_do_connect_fail(lmqtt_client_t *client,
    lmqtt_connect_t *connect)
{
    return 0;
}

static int client_do_connect(lmqtt_client_t *client, lmqtt_connect_t *connect)
{
    lmqtt_tx_buffer_connect(&client->tx_state, connect);

    client->internal.connect = client_do_connect_fail;
    client->internal.rx_callbacks.on_connack = client_on_connack;

    return 1;
}

static int client_do_pingreq_fail(lmqtt_client_t *client)
{
    return 0;
}

static int client_do_pingreq(lmqtt_client_t *client)
{
    lmqtt_tx_buffer_pingreq(&client->tx_state);

    return 1;
}

lmqtt_io_status_t client_keep_alive(lmqtt_client_t *client)
{
    client->internal.pingreq(client);
    return LMQTT_IO_STATUS_READY;
}

/******************************************************************************
 * lmqtt_client_t PUBLIC functions
 ******************************************************************************/

void lmqtt_client_initialize(lmqtt_client_t *client)
{
    memset(client, 0, sizeof(*client));

    client->internal.connect = client_do_connect;
    client->internal.pingreq = client_do_pingreq_fail;
    client->internal.rx_callbacks.on_connack = client_on_connack_fail;
    client->internal.rx_callbacks.on_pingresp = client_on_pingresp_fail;

    client->rx_state.callbacks = &client->internal.rx_callbacks;
    client->rx_state.callbacks_data = client;
}

int lmqtt_client_connect(lmqtt_client_t *client, lmqtt_connect_t *connect)
{
    return client->internal.connect(client, connect);
}

void lmqtt_client_set_on_connect(lmqtt_client_t *client,
    lmqtt_client_on_connect_t on_connect, void *on_connect_data)
{
    client->on_connect = on_connect;
    client->on_connect_data = on_connect_data;
}
