#include <lightmqtt/io.h>
#include <string.h>

/******************************************************************************
 * lmqtt_input_t
 ******************************************************************************/

typedef struct _lmqtt_input_t {
    lmqtt_read_t read;
    void *data;
} lmqtt_input_t;

static lmqtt_io_result_t input_read(lmqtt_input_t *input, u8 *buf,
    int *buf_pos, int buf_len, int *cnt)
{
    lmqtt_io_result_t result;

    result = input->read(input->data, &buf[*buf_pos], buf_len - *buf_pos, cnt);
    *buf_pos += *cnt;

    return result;
}

/******************************************************************************
 * lmqtt_output_t
 ******************************************************************************/

typedef struct _lmqtt_output_t {
    lmqtt_write_t write;
    void *data;
} lmqtt_output_t;

static lmqtt_io_result_t output_write(lmqtt_output_t *output, u8 *buf,
    int *buf_pos, int *cnt)
{
    lmqtt_io_result_t result;

    result = output->write(output->data, buf, *buf_pos, cnt);
    memmove(&buf[0], &buf[*cnt], *buf_pos - *cnt);
    *buf_pos -= *cnt;

    return result;
}

/******************************************************************************
 * lmqtt_client_t PRIVATE functions
 ******************************************************************************/

static void client_set_state_initial(lmqtt_client_t *client);
static void client_set_state_connecting(lmqtt_client_t *client);
static void client_set_state_connected(lmqtt_client_t *client);
static void client_set_state_failed(lmqtt_client_t *client);

static int client_buffer_check(lmqtt_client_t *client, lmqtt_io_result_t io_res,
    int *cnt, lmqtt_io_status_t block_status, lmqtt_io_status_t *transfer_res)
{
    if (io_res == LMQTT_IO_AGAIN) {
        /* if both input and output block prefer LMQTT_IO_STATUS_BLOCK_CONN */
        if (*transfer_res != LMQTT_IO_STATUS_BLOCK_CONN)
            *transfer_res = block_status;
        return 0;
    }

    if (io_res == LMQTT_IO_ERROR || client->failed) {
        *transfer_res = LMQTT_IO_STATUS_ERROR;
        client_set_state_failed(client);
        return 0;
    }

    return *cnt > 0;
}

static void client_flush_store(lmqtt_client_t *client, lmqtt_store_t *store)
{
    lmqtt_store_value_t value;

    while (lmqtt_store_shift(store, NULL, &value)) {
        if (value.callback)
            value.callback(value.callback_data, value.value);
    }
}

static void client_cleanup_stores(lmqtt_client_t *client, int keep_session)
{
    int i = 0;
    int class;
    lmqtt_store_value_t value;

    if (keep_session) {
        while (lmqtt_store_get_at(&client->main_store, i, &class, NULL)) {
            if (class == LMQTT_CLASS_PINGREQ || class == LMQTT_CLASS_DISCONNECT)
                lmqtt_store_delete_at(&client->main_store, i);
            else
                i++;
        }
    } else {
        client_flush_store(client, &client->main_store);
    }

    client_flush_store(client, &client->connect_store);
}

static void client_set_current_store(lmqtt_client_t *client,
    lmqtt_store_t *store)
{
    client->current_store = store;
    client->rx_state.store = store;
    client->tx_state.store = store;
}

static lmqtt_io_status_t client_buffer_transfer(lmqtt_client_t *client,
    lmqtt_input_t *input, lmqtt_io_status_t reader_block,
    lmqtt_output_t *output, lmqtt_io_status_t writer_block,
    u8 *buf, int *buf_pos, int buf_len)
{
    int read_allowed = 1;
    int write_allowed = 1;
    lmqtt_io_status_t result = LMQTT_IO_STATUS_READY;

    if (client->failed)
        return LMQTT_IO_STATUS_ERROR;

    while (read_allowed || write_allowed) {
        int cnt;

        read_allowed = read_allowed && !client->failed && *buf_pos < buf_len &&
            client_buffer_check(client, input_read(input,
                buf, buf_pos, buf_len, &cnt), &cnt, reader_block, &result);

        write_allowed = write_allowed && !client->failed && *buf_pos > 0 &&
            client_buffer_check(client, output_write(output,
                buf, buf_pos, &cnt), &cnt, writer_block, &result);
    }

    if (result == LMQTT_IO_STATUS_READY)
        client_set_state_initial(client);

    return result;
}

static lmqtt_io_result_t client_decode_wrapper(void *data, u8 *buf, int buf_len,
    int *bytes_read)
{
    return lmqtt_rx_buffer_decode((lmqtt_rx_buffer_t *) data, buf, buf_len,
        bytes_read);
}

static lmqtt_io_result_t client_encode_wrapper(void *data, u8 *buf, int buf_len,
    int *bytes_written)
{
    return lmqtt_tx_buffer_encode((lmqtt_tx_buffer_t *) data, buf, buf_len,
        bytes_written);
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
lmqtt_io_status_t client_process_input(lmqtt_client_t *client)
{
    lmqtt_input_t input = { client->callbacks.read, client->callbacks.data };
    lmqtt_output_t output = { client_decode_wrapper, &client->rx_state };

    return client_buffer_transfer(client,
        &input, LMQTT_IO_STATUS_BLOCK_CONN,
        &output, LMQTT_IO_STATUS_BLOCK_DATA,
        client->read_buf, &client->read_buf_pos, sizeof(client->read_buf));
}

/*
 * TODO: review how lmqtt_tx_buffer_encode() should handle cases where some read
 * would block, cases where there's no data to encode and cases where the buffer
 * is not enough to encode the whole command.
 */
lmqtt_io_status_t client_process_output(lmqtt_client_t *client)
{
    lmqtt_input_t input = { client_encode_wrapper, &client->tx_state };
    lmqtt_output_t output = { client->callbacks.write, client->callbacks.data };

    return client_buffer_transfer(client,
        &input, LMQTT_IO_STATUS_BLOCK_DATA,
        &output, LMQTT_IO_STATUS_BLOCK_CONN,
        client->write_buf, &client->write_buf_pos, sizeof(client->write_buf));
}

lmqtt_io_status_t client_keep_alive(lmqtt_client_t *client)
{
    int cnt;
    long s, ns;

    if (client->failed)
        return LMQTT_IO_STATUS_ERROR;

    if (!lmqtt_store_get_timeout(client->current_store, &cnt, &s, &ns) ||
            s != 0 || ns != 0)
        return LMQTT_IO_STATUS_READY;

    if (cnt > 0) {
        client_set_state_failed(client);
        return LMQTT_IO_STATUS_ERROR;
    }

    client->internal.pingreq(client);
    return LMQTT_IO_STATUS_READY;
}

static int client_subscribe_with_class(lmqtt_client_t *client,
    lmqtt_subscribe_t *subscribe, lmqtt_class_t class,
    lmqtt_store_entry_callback_t cb)
{
    u16 packet_id;
    lmqtt_store_value_t value;

    if (!lmqtt_subscribe_validate(subscribe))
        return 0;

    packet_id = lmqtt_store_get_id(&client->main_store);
    subscribe->packet_id = packet_id;

    value.value = subscribe;
    value.callback = cb;
    value.callback_data = client;

    return lmqtt_store_append(&client->main_store, class, packet_id, &value);
}

static int client_on_connack(void *data, lmqtt_connect_t *connect)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    if (client->closed) {
        if (client->on_connect)
            client->on_connect(client->on_connect_data, connect, 0);
    } else if (connect->response.return_code == LMQTT_CONNACK_RC_ACCEPTED) {
        client->clean_session = connect->clean_session;
        client->main_store.keep_alive = connect->keep_alive;
        client_set_state_connected(client);

        if (client->on_connect)
            client->on_connect(client->on_connect_data, connect, 1);
    } else {
        client_set_state_initial(client);

        if (client->on_connect)
            client->on_connect(client->on_connect_data, connect, 0);
    }

    return 1;
}

static int client_on_suback(void *data, lmqtt_subscribe_t *subscribe)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    if (client->on_subscribe)
        client->on_subscribe(client->on_subscribe_data, subscribe,
            !client->closed);

    return 1;
}

static int client_on_unsuback(void *data, lmqtt_subscribe_t *subscribe)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    if (client->on_unsubscribe)
        client->on_unsubscribe(client->on_unsubscribe_data, subscribe,
            !client->closed);

    return 1;
}

static int client_on_publish(void *data, lmqtt_publish_t *publish)
{
    lmqtt_client_t *client = (lmqtt_client_t *) data;

    if (client->on_publish)
        client->on_publish(client->on_publish_data, publish,
            !client->closed);

    return 1;
}

static int client_on_pingresp(void *data, void *unused)
{
    return 1;
}

static int client_do_connect_fail(lmqtt_client_t *client,
    lmqtt_connect_t *connect)
{
    return 0;
}

static int client_do_connect(lmqtt_client_t *client, lmqtt_connect_t *connect)
{
    lmqtt_store_value_t value;

    if (!lmqtt_connect_validate(connect))
        return 0;

    value.value = connect;
    value.callback = (lmqtt_store_entry_callback_t) &client_on_connack;
    value.callback_data = client;

    if (!lmqtt_store_append(&client->connect_store, LMQTT_CLASS_CONNECT, 0,
            &value))
        return 0;

    client_set_state_connecting(client);
    return 1;
}

static int client_do_subscribe_fail(lmqtt_client_t *client,
    lmqtt_subscribe_t *subscribe)
{
    return 0;
}

static int client_do_subscribe(lmqtt_client_t *client,
    lmqtt_subscribe_t *subscribe)
{
    return client_subscribe_with_class(client,
        subscribe, LMQTT_CLASS_SUBSCRIBE,
        (lmqtt_store_entry_callback_t) &client_on_suback);
}

static int client_do_unsubscribe_fail(lmqtt_client_t *client,
    lmqtt_subscribe_t *subscribe)
{
    return 0;
}

static int client_do_unsubscribe(lmqtt_client_t *client,
    lmqtt_subscribe_t *subscribe)
{
    return client_subscribe_with_class(client,
        subscribe, LMQTT_CLASS_UNSUBSCRIBE,
        (lmqtt_store_entry_callback_t) &client_on_unsuback);
}

static int client_do_publish_fail(lmqtt_client_t *client,
    lmqtt_publish_t *publish)
{
    return 0;
}

static int client_do_publish(lmqtt_client_t *client, lmqtt_publish_t *publish)
{
    int class;
    int qos = publish->qos;
    lmqtt_store_value_t value;

    if (!lmqtt_publish_validate(publish))
        return 0;

    if (qos == 0) {
        class = LMQTT_CLASS_PUBLISH_0;
        publish->packet_id = 0;
    } else {
        class = qos == 1 ? LMQTT_CLASS_PUBLISH_1 : LMQTT_CLASS_PUBLISH_2;
        publish->packet_id = lmqtt_store_get_id(&client->main_store);
    }

    value.value = publish;
    value.callback = (lmqtt_store_entry_callback_t) &client_on_publish;
    value.callback_data = client;

    return lmqtt_store_append(&client->main_store, class, publish->packet_id,
        &value);
}

static int client_do_pingreq_fail(lmqtt_client_t *client)
{
    return 0;
}

static int client_do_pingreq(lmqtt_client_t *client)
{
    lmqtt_store_value_t value;

    value.value = NULL;
    value.callback = &client_on_pingresp;
    value.callback_data = client;

    return lmqtt_store_append(&client->main_store, LMQTT_CLASS_PINGREQ, 0,
        &value);
}

static int client_do_disconnect_fail(lmqtt_client_t *client)
{
    return 0;
}

static int client_do_disconnect(lmqtt_client_t *client)
{
    return lmqtt_store_append(&client->main_store, LMQTT_CLASS_DISCONNECT, 0,
        NULL);
}

static void client_set_state_initial(lmqtt_client_t *client)
{
    client->closed = 1;
    client->failed = 0;

    client_set_current_store(client, &client->connect_store);
    client_cleanup_stores(client, !client->clean_session);

    lmqtt_tx_buffer_finish(&client->tx_state);

    client->internal.connect = client_do_connect;
    client->internal.subscribe = client_do_subscribe_fail;
    client->internal.unsubscribe = client_do_unsubscribe_fail;
    client->internal.publish = client_do_publish_fail;
    client->internal.pingreq = client_do_pingreq_fail;
    client->internal.disconnect = client_do_disconnect_fail;
}

static void client_set_state_connecting(lmqtt_client_t *client)
{
    client->closed = 0;
    client->failed = 0;

    lmqtt_rx_buffer_reset(&client->rx_state);
    lmqtt_tx_buffer_reset(&client->tx_state);
    /* TODO: When lmqtt_rx_buffer_decode implements handling of blocking writes
       (for example, when writing the contents of a PUBLISH packet to a file) it
       may be possible for client->read_buf_pos to not be 0 before a reconnect.
       We need to write a test-case for that, like the one for
       client->write_buf_pos (see `should_reset_output_buffer_on_reconnect` in
       check_client_commands.c). */
    client->read_buf_pos = 0;
    client->write_buf_pos = 0;

    client->internal.connect = client_do_connect_fail;
}

static void client_set_state_connected(lmqtt_client_t *client)
{
    client->closed = 0;
    client->failed = 0;

    client_set_current_store(client, &client->main_store);
    client_cleanup_stores(client, !client->clean_session);

    lmqtt_store_unmark_all(&client->main_store);

    client->internal.subscribe = client_do_subscribe;
    client->internal.unsubscribe = client_do_unsubscribe;
    client->internal.publish = client_do_publish;
    client->internal.pingreq = client_do_pingreq;
    client->internal.disconnect = client_do_disconnect;
}

static void client_set_state_failed(lmqtt_client_t *client)
{
    client->closed = 1;
    client->failed = 1;

    client->internal.connect = client_do_connect_fail;
    client->internal.subscribe = client_do_subscribe_fail;
    client->internal.unsubscribe = client_do_unsubscribe_fail;
    client->internal.publish = client_do_publish_fail;
    client->internal.pingreq = client_do_pingreq_fail;
    client->internal.disconnect = client_do_disconnect_fail;
}

/******************************************************************************
 * lmqtt_client_t PUBLIC functions
 ******************************************************************************/

void lmqtt_client_initialize(lmqtt_client_t *client, lmqtt_callbacks_t
    *callbacks)
{
    memset(client, 0, sizeof(*client));

    memcpy(&client->callbacks, callbacks, sizeof(*callbacks));
    client->main_store.get_time = callbacks->get_time;
    client->connect_store.get_time = callbacks->get_time;

    client_set_state_initial(client);
}

void lmqtt_client_finalize(lmqtt_client_t *client)
{
    client_set_state_failed(client);

    lmqtt_rx_buffer_finish(&client->rx_state);
    client_cleanup_stores(client, 0);
}

int lmqtt_client_connect(lmqtt_client_t *client, lmqtt_connect_t *connect)
{
    return client->internal.connect(client, connect);
}

int lmqtt_client_subscribe(lmqtt_client_t *client, lmqtt_subscribe_t *subscribe)
{
    return client->internal.subscribe(client, subscribe);
}

int lmqtt_client_unsubscribe(lmqtt_client_t *client,
    lmqtt_subscribe_t *subscribe)
{
    return client->internal.unsubscribe(client, subscribe);
}

int lmqtt_client_publish(lmqtt_client_t *client, lmqtt_publish_t *publish)
{
    return client->internal.publish(client, publish);
}

int lmqtt_client_disconnect(lmqtt_client_t *client)
{
    return client->internal.disconnect(client);
}

void lmqtt_client_set_on_connect(lmqtt_client_t *client,
    lmqtt_client_on_connect_t on_connect, void *on_connect_data)
{
    client->on_connect = on_connect;
    client->on_connect_data = on_connect_data;
}

void lmqtt_client_set_on_subscribe(lmqtt_client_t *client,
    lmqtt_client_on_subscribe_t on_subscribe, void *on_subscribe_data)
{
    client->on_subscribe = on_subscribe;
    client->on_subscribe_data = on_subscribe_data;
}

void lmqtt_client_set_on_unsubscribe(lmqtt_client_t *client,
    lmqtt_client_on_unsubscribe_t on_unsubscribe, void *on_unsubscribe_data)
{
    client->on_unsubscribe = on_unsubscribe;
    client->on_unsubscribe_data = on_unsubscribe_data;
}

void lmqtt_client_set_on_publish(lmqtt_client_t *client,
    lmqtt_client_on_publish_t on_publish, void *on_publish_data)
{
    client->on_publish = on_publish;
    client->on_publish_data = on_publish_data;
}

void lmqtt_client_set_default_timeout(lmqtt_client_t *client, long secs)
{
    client->main_store.timeout = secs;
    client->connect_store.timeout = secs;
}

int lmqtt_client_get_timeout(lmqtt_client_t *client, long *secs, long *nsecs)
{
    int cnt;

    return lmqtt_store_get_timeout(&client->main_store, &cnt, secs, nsecs);
}
