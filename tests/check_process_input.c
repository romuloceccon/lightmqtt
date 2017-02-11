#include "check_lightmqtt.h"

#define decode_rx_buffer _original_decode_rx_buffer
#include "../src/lightmqtt.c"
#undef decode_rx_buffer

typedef struct _TestConnection {
    u8 buf[LMQTT_RX_BUFFER_SIZE * 2];
    int pos;
    int len;
    int call_count;
} TestConnection;

typedef struct _TestRxBuffer {
    u8 buf[LMQTT_RX_BUFFER_SIZE * 2];
    int pos;
    int len;
    int call_count;
} TestRxBuffer;

static TestRxBuffer rx_buffer;

static int read_test_buf(void *data, u8 *buf, int buf_len, int *bytes_read)
{
    TestConnection *connection = (TestConnection *) data;
    int cnt = connection->len - connection->pos;
    if (cnt > buf_len)
        cnt = buf_len;
    memcpy(buf, &connection->buf[connection->pos], cnt);
    *bytes_read = cnt;
    connection->pos += cnt;
    connection->call_count += 1;
    return cnt > 0 ? LMQTT_ERR_FINISHED : LMQTT_ERR_AGAIN;
}

static int decode_rx_buffer(LMqttRxBufferState *state, u8 *buf, int buf_len,
    int *bytes_read)
{
    int cnt = buf_len;
    if (cnt > rx_buffer.len - rx_buffer.pos)
        cnt = rx_buffer.len - rx_buffer.pos;
    memcpy(&rx_buffer.buf[rx_buffer.pos], buf, cnt);
    *bytes_read = cnt;
    rx_buffer.pos += cnt;
    rx_buffer.call_count += 1;
    return cnt >= buf_len ? LMQTT_DECODE_FINISHED : LMQTT_DECODE_AGAIN;
}

#include "../src/lightmqtt_client.c"

START_TEST(should_process_input_without_data)
{
    LMqttClient client;
    TestConnection connection;

    memset(&client, 0, sizeof(client));
    memset(&connection, 0, sizeof(connection));
    memset(&rx_buffer, 0, sizeof(rx_buffer));

    client.data = &connection;
    client.read = read_test_buf;
    rx_buffer.len = sizeof(rx_buffer.buf);

    process_input(&client);

    ck_assert_int_eq(0, connection.pos);
    ck_assert_int_eq(1, connection.call_count);
    ck_assert_int_eq(0, rx_buffer.pos);
    ck_assert_int_eq(0, rx_buffer.call_count);
}
END_TEST

START_TEST(should_process_input_with_complete_read_and_complete_process)
{
    LMqttClient client;
    TestConnection connection;

    memset(&client, 0, sizeof(client));
    memset(&connection, 0, sizeof(connection));
    memset(&rx_buffer, 0, sizeof(rx_buffer));

    client.data = &connection;
    client.read = read_test_buf;
    rx_buffer.len = sizeof(rx_buffer.buf);
    memset(&connection.buf, 0xf, 5);
    connection.len = 5;

    process_input(&client);

    ck_assert_int_eq(5, connection.pos);
    ck_assert_int_eq(2, connection.call_count);
    ck_assert_int_eq(5, rx_buffer.pos);
    ck_assert_int_eq(1, rx_buffer.call_count);
}
END_TEST

START_TEST(should_consume_read_buffer_if_process_interrupts)
{
    LMqttClient client;
    TestConnection connection;
    int i;

    memset(&client, 0, sizeof(client));
    memset(&connection, 0, sizeof(connection));
    memset(&rx_buffer, 0, sizeof(rx_buffer));

    client.data = &connection;
    client.read = read_test_buf;
    rx_buffer.len = LMQTT_RX_BUFFER_SIZE / 2;
    connection.len = LMQTT_RX_BUFFER_SIZE / 4 * 5;
    for (i = 0; i < LMQTT_RX_BUFFER_SIZE / 4 * 5; i++)
        connection.buf[i] = i % 199;

    process_input(&client);

    /* will process half of a buffer; should read whole input (5/4 of a buffer),
       and leave 3/4 of a buffer to process next time */
    ck_assert_int_eq(LMQTT_RX_BUFFER_SIZE / 4 * 5, connection.pos);
    ck_assert_int_eq(3, connection.call_count);
    ck_assert_int_eq(LMQTT_RX_BUFFER_SIZE / 2, rx_buffer.pos);
    ck_assert_int_eq(1, rx_buffer.call_count);
    ck_assert_uint_eq(0 % 199, rx_buffer.buf[0]);
    ck_assert_uint_eq((LMQTT_RX_BUFFER_SIZE / 2 - 1) % 199,
        rx_buffer.buf[LMQTT_RX_BUFFER_SIZE / 2 - 1]);
}
END_TEST

START_TEST(should_fill_read_buffer_if_process_interrupts)
{
    LMqttClient client;
    TestConnection connection;
    int i;

    memset(&client, 0, sizeof(client));
    memset(&connection, 0, sizeof(connection));
    memset(&rx_buffer, 0, sizeof(rx_buffer));

    client.data = &connection;
    client.read = read_test_buf;
    rx_buffer.len = LMQTT_RX_BUFFER_SIZE / 2;
    connection.len = sizeof(connection.buf);
    for (i = 0; i < sizeof(connection.buf); i++)
        connection.buf[i] = i % 199;

    process_input(&client);

    /* will process half of a buffer; should read at most one and a half
       buffer */
    ck_assert_int_eq(LMQTT_RX_BUFFER_SIZE / 2 * 3, connection.pos);
    ck_assert_int_eq(2, connection.call_count);
    ck_assert_int_eq(LMQTT_RX_BUFFER_SIZE / 2, rx_buffer.pos);
    ck_assert_int_eq(1, rx_buffer.call_count);
    ck_assert_uint_eq(0 % 199, rx_buffer.buf[0]);
    ck_assert_uint_eq((LMQTT_RX_BUFFER_SIZE / 2 - 1) % 199,
        rx_buffer.buf[LMQTT_RX_BUFFER_SIZE / 2 - 1]);
}
END_TEST

START_TEST(should_process_remaining_input_from_previous_call)
{
    LMqttClient client;
    TestConnection connection;
    int i;
    static int s = LMQTT_RX_BUFFER_SIZE / 8;

    memset(&client, 0, sizeof(client));
    memset(&connection, 0, sizeof(connection));
    memset(&rx_buffer, 0, sizeof(rx_buffer));

    client.data = &connection;
    client.read = read_test_buf;
    rx_buffer.len = s;
    connection.len = sizeof(connection.buf);
    for (i = 0; i < sizeof(connection.buf); i++)
        connection.buf[i] = i % 199;

    process_input(&client);

    ck_assert_int_eq(s, rx_buffer.pos);
    ck_assert_int_eq(1, rx_buffer.call_count);
    ck_assert_uint_eq(0 % 199, rx_buffer.buf[0]);
    ck_assert_uint_eq((s - 1) % 199, rx_buffer.buf[s - 1]);

    rx_buffer.pos = 0;
    process_input(&client);

    ck_assert_int_eq(s, rx_buffer.pos);
    ck_assert_int_eq(2, rx_buffer.call_count);
    ck_assert_uint_eq(s % 199, rx_buffer.buf[0]);
    ck_assert_uint_eq((s + s - 1) % 199, rx_buffer.buf[s - 1]);
}
END_TEST

START_TCASE("Process input")
{
    ADD_TEST(should_process_input_without_data);
    ADD_TEST(should_consume_read_buffer_if_process_interrupts);
    ADD_TEST(should_fill_read_buffer_if_process_interrupts);
    ADD_TEST(should_process_remaining_input_from_previous_call);
}
END_TCASE
