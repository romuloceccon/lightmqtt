#include "check_lightmqtt.h"

#include "../src/lmqtt_io.c"

typedef struct _TestConnection {
    u8 buf[LMQTT_TX_BUFFER_SIZE * 2];
    int pos;
    int len;
    int call_count;
} TestConnection;

typedef struct _TestTxBuffer {
    u8 buf[LMQTT_TX_BUFFER_SIZE * 2];
    int pos;
    int len;
    int call_count;
} TestTxBuffer;

static TestTxBuffer tx_buffer;

static int write_test_buf(void *data, u8 *buf, int buf_len, int *bytes_written)
{
    TestConnection *connection = (TestConnection *) data;
    int cnt = buf_len;
    if (cnt > connection->len - connection->pos)
        cnt = connection->len - connection->pos;
    memcpy(&connection->buf[connection->pos], buf, cnt);
    *bytes_written = cnt;
    connection->pos += cnt;
    connection->call_count += 1;
    return cnt > 0 ? LMQTT_ERR_FINISHED : LMQTT_ERR_AGAIN;
}

/*
 * TODO: this implementation is not returning in the same way as
 * decode_rx_buffer() (or the original implementation of encode_tx_buffer()).
 * This should be better clarified because this prevents the following test-
 * cases to be perfectly symetric with check_process_input.c
 * (connection.call_count is 2 varios tests below, but should be 1 to respect
 * symetry).
 */
int encode_tx_buffer(lmqtt_tx_buffer_state_t *state, u8 *buf, int buf_len,
    int *bytes_written)
{
    int cnt = tx_buffer.len - tx_buffer.pos;
    if (cnt > buf_len)
        cnt = buf_len;
    memcpy(buf, &tx_buffer.buf[tx_buffer.pos], cnt);
    *bytes_written = cnt;
    tx_buffer.pos += cnt;
    tx_buffer.call_count += 1;
    return cnt > 0 ? LMQTT_ENCODE_FINISHED : LMQTT_ENCODE_AGAIN;
}

START_TEST(should_process_output_without_data)
{
    lmqtt_client_t client;
    TestConnection connection;

    memset(&client, 0, sizeof(client));
    memset(&connection, 0, sizeof(connection));
    memset(&tx_buffer, 0, sizeof(tx_buffer));

    client.data = &connection;
    client.write = write_test_buf;
    connection.len = sizeof(connection.buf);

    process_output(&client);

    ck_assert_int_eq(0, connection.pos);
    ck_assert_int_eq(0, connection.call_count);
    ck_assert_int_eq(0, tx_buffer.pos);
    ck_assert_int_eq(1, tx_buffer.call_count);
}
END_TEST

START_TEST(should_process_output_with_complete_build_and_complete_write)
{
    lmqtt_client_t client;
    TestConnection connection;

    memset(&client, 0, sizeof(client));
    memset(&connection, 0, sizeof(connection));
    memset(&tx_buffer, 0, sizeof(tx_buffer));

    client.data = &connection;
    client.write = write_test_buf;
    connection.len = sizeof(connection.buf);
    memset(&tx_buffer.buf, 0xf, 5);
    tx_buffer.len = 5;

    process_output(&client);

    ck_assert_int_eq(5, connection.pos);
    ck_assert_int_eq(1, connection.call_count);
    ck_assert_int_eq(5, tx_buffer.pos);
    ck_assert_int_eq(2, tx_buffer.call_count);
}
END_TEST

START_TEST(should_consume_write_buffer_if_build_interrupts)
{
    lmqtt_client_t client;
    TestConnection connection;
    int i;

    memset(&client, 0, sizeof(client));
    memset(&connection, 0, sizeof(connection));
    memset(&tx_buffer, 0, sizeof(tx_buffer));

    client.data = &connection;
    client.write = write_test_buf;
    connection.len = LMQTT_TX_BUFFER_SIZE / 2;
    tx_buffer.len = LMQTT_TX_BUFFER_SIZE / 4 * 5;
    for (i = 0; i < LMQTT_TX_BUFFER_SIZE / 4 * 5; i++)
        tx_buffer.buf[i] = i % 199;

    process_output(&client);

    ck_assert_int_eq(LMQTT_TX_BUFFER_SIZE / 2, connection.pos);
    ck_assert_int_eq(2, connection.call_count);
    ck_assert_int_eq(LMQTT_TX_BUFFER_SIZE / 4 * 5, tx_buffer.pos);
    ck_assert_int_eq(3, tx_buffer.call_count);
    ck_assert_uint_eq(0 % 199, connection.buf[0]);
    ck_assert_uint_eq((LMQTT_TX_BUFFER_SIZE / 2 - 1) % 199,
        connection.buf[LMQTT_TX_BUFFER_SIZE / 2 - 1]);
}
END_TEST

START_TEST(should_fill_write_buffer_if_build_interrupts)
{
    lmqtt_client_t client;
    TestConnection connection;
    int i;

    memset(&client, 0, sizeof(client));
    memset(&connection, 0, sizeof(connection));
    memset(&tx_buffer, 0, sizeof(tx_buffer));

    client.data = &connection;
    client.write = write_test_buf;
    connection.len = LMQTT_TX_BUFFER_SIZE / 2;
    tx_buffer.len = sizeof(tx_buffer.buf);
    for (i = 0; i < sizeof(tx_buffer.buf); i++)
        tx_buffer.buf[i] = i % 199;

    process_output(&client);

    ck_assert_int_eq(LMQTT_TX_BUFFER_SIZE / 2, connection.pos);
    ck_assert_int_eq(2, connection.call_count);
    ck_assert_int_eq(LMQTT_TX_BUFFER_SIZE / 2 * 3, tx_buffer.pos);
    ck_assert_int_eq(2, tx_buffer.call_count);
    ck_assert_uint_eq(0 % 199, connection.buf[0]);
    ck_assert_uint_eq((LMQTT_TX_BUFFER_SIZE / 2 - 1) % 199,
        connection.buf[LMQTT_TX_BUFFER_SIZE / 2 - 1]);
}
END_TEST

START_TEST(should_process_remaining_output_from_previous_call)
{
    lmqtt_client_t client;
    TestConnection connection;
    int i;
    static int s = LMQTT_TX_BUFFER_SIZE / 8;

    memset(&client, 0, sizeof(client));
    memset(&connection, 0, sizeof(connection));
    memset(&tx_buffer, 0, sizeof(tx_buffer));

    client.data = &connection;
    client.write = write_test_buf;
    connection.len = s;
    tx_buffer.len = sizeof(tx_buffer.buf);
    for (i = 0; i < sizeof(tx_buffer.buf); i++)
        tx_buffer.buf[i] = i % 199;

    process_output(&client);

    ck_assert_int_eq(s, connection.pos);
    ck_assert_int_eq(2, connection.call_count);
    ck_assert_uint_eq(0 % 199, connection.buf[0]);
    ck_assert_uint_eq((s - 1) % 199, connection.buf[s - 1]);

    connection.pos = 0;
    process_output(&client);

    ck_assert_int_eq(s, connection.pos);
    ck_assert_int_eq(4, connection.call_count);
    ck_assert_uint_eq(s % 199, connection.buf[0]);
    ck_assert_uint_eq((s + s - 1) % 199, connection.buf[s - 1]);
}
END_TEST

START_TCASE("Process output")
{
    ADD_TEST(should_process_output_without_data);
    ADD_TEST(should_process_output_with_complete_build_and_complete_write);
    ADD_TEST(should_consume_write_buffer_if_build_interrupts);
    ADD_TEST(should_fill_write_buffer_if_build_interrupts);
    ADD_TEST(should_process_remaining_output_from_previous_call);
}
END_TCASE
