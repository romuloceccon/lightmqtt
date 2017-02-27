#include "check_lightmqtt.h"

#include "../src/lmqtt_io.c"

static test_buffer_t test_destination;

static lmqtt_io_result_t read_test_buf(void *data, u8 *buf, int buf_len,
    int *bytes_read)
{
    test_buffer_t *source = (test_buffer_t *) data;

    return move_buf(source, buf, &source->buf[source->pos], buf_len,
        bytes_read);
}

lmqtt_io_result_t lmqtt_rx_buffer_decode(lmqtt_rx_buffer_t *state, u8 *buf,
    int buf_len, int *bytes_read)
{
    return move_buf(&test_destination,
        &test_destination.buf[test_destination.pos], buf, buf_len, bytes_read);
}

START_TEST(should_process_input_without_data)
{
    lmqtt_client_t client;
    test_buffer_t source;

    memset(&client, 0, sizeof(client));
    memset(&source, 0, sizeof(source));
    memset(&test_destination, 0, sizeof(test_destination));

    client.data = &source;
    client.read = read_test_buf;
    test_destination.len = sizeof(test_destination.buf);
    test_destination.available_len = test_destination.len;

    process_input(&client);

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

    process_input(&client);

    ck_assert_int_eq(5, source.pos);
    ck_assert_int_eq(2, source.call_count);
    ck_assert_int_eq(5, test_destination.pos);
    ck_assert_int_eq(1, test_destination.call_count);
}
END_TEST

START_TEST(should_consume_read_buffer_if_process_interrupts)
{
    lmqtt_client_t client;
    test_buffer_t source;
    int i;

    memset(&client, 0, sizeof(client));
    memset(&source, 0, sizeof(source));
    memset(&test_destination, 0, sizeof(test_destination));

    client.data = &source;
    client.read = read_test_buf;
    test_destination.len = LMQTT_RX_BUFFER_SIZE / 2;
    test_destination.available_len = test_destination.len;
    source.len = LMQTT_RX_BUFFER_SIZE / 4 * 5;
    source.available_len = source.len;
    for (i = 0; i < LMQTT_RX_BUFFER_SIZE / 4 * 5; i++)
        source.buf[i] = i % 199;

    process_input(&client);

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

START_TEST(should_fill_read_buffer_if_process_interrupts)
{
    lmqtt_client_t client;
    test_buffer_t source;
    int i;

    memset(&client, 0, sizeof(client));
    memset(&source, 0, sizeof(source));
    memset(&test_destination, 0, sizeof(test_destination));

    client.data = &source;
    client.read = read_test_buf;
    test_destination.len = LMQTT_RX_BUFFER_SIZE / 2;
    test_destination.available_len = test_destination.len;
    source.len = sizeof(source.buf);
    source.available_len = source.len;
    for (i = 0; i < sizeof(source.buf); i++)
        source.buf[i] = i % 199;

    process_input(&client);

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

    memset(&client, 0, sizeof(client));
    memset(&source, 0, sizeof(source));
    memset(&test_destination, 0, sizeof(test_destination));

    client.data = &source;
    client.read = read_test_buf;
    test_destination.len = s;
    test_destination.available_len = test_destination.len;
    source.len = sizeof(source.buf);
    source.available_len = source.len;
    for (i = 0; i < sizeof(source.buf); i++)
        source.buf[i] = i % 199;

    process_input(&client);

    ck_assert_int_eq(s, test_destination.pos);
    ck_assert_int_eq(2, test_destination.call_count);
    ck_assert_uint_eq(0 % 199, test_destination.buf[0]);
    ck_assert_uint_eq((s - 1) % 199, test_destination.buf[s - 1]);

    test_destination.pos = 0;
    process_input(&client);

    ck_assert_int_eq(s, test_destination.pos);
    ck_assert_int_eq(4, test_destination.call_count);
    ck_assert_uint_eq(s % 199, test_destination.buf[0]);
    ck_assert_uint_eq((s + s - 1) % 199, test_destination.buf[s - 1]);
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
