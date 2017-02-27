#include "check_lightmqtt.h"

#include "../src/lmqtt_io.c"

static test_buffer_t test_source;

static lmqtt_io_result_t write_test_buf(void *data, u8 *buf, int buf_len,
    int *bytes_written)
{
    test_buffer_t *destination = (test_buffer_t *) data;

    return move_buf(destination, &destination->buf[destination->pos], buf,
        buf_len, bytes_written);
}

lmqtt_io_result_t lmqtt_tx_buffer_encode(lmqtt_tx_buffer_t *state, u8 *buf,
    int buf_len, int *bytes_written)
{
    return move_buf(&test_source, buf, &test_source.buf[test_source.pos],
        buf_len, bytes_written);
}

START_TEST(should_process_output_without_data)
{
    lmqtt_client_t client;
    test_buffer_t destination;

    memset(&client, 0, sizeof(client));
    memset(&destination, 0, sizeof(destination));
    memset(&test_source, 0, sizeof(test_source));

    client.data = &destination;
    client.write = write_test_buf;
    destination.len = sizeof(destination.buf);
    destination.available_len = destination.len;

    process_output(&client);

    ck_assert_int_eq(0, destination.pos);
    ck_assert_int_eq(0, destination.call_count);
    ck_assert_int_eq(0, test_source.pos);
    ck_assert_int_eq(1, test_source.call_count);
}
END_TEST

START_TEST(should_process_output_with_complete_build_and_complete_write)
{
    lmqtt_client_t client;
    test_buffer_t destination;

    memset(&client, 0, sizeof(client));
    memset(&destination, 0, sizeof(destination));
    memset(&test_source, 0, sizeof(test_source));

    client.data = &destination;
    client.write = write_test_buf;
    destination.len = sizeof(destination.buf);
    destination.available_len = destination.len;
    memset(&test_source.buf, 0xf, 5);
    test_source.len = 5;
    test_source.available_len = test_source.len;

    process_output(&client);

    ck_assert_int_eq(5, destination.pos);
    ck_assert_int_eq(1, destination.call_count);
    ck_assert_int_eq(5, test_source.pos);
    ck_assert_int_eq(2, test_source.call_count);
}
END_TEST

START_TEST(should_consume_write_buffer_if_build_interrupts)
{
    lmqtt_client_t client;
    test_buffer_t destination;
    int i;

    memset(&client, 0, sizeof(client));
    memset(&destination, 0, sizeof(destination));
    memset(&test_source, 0, sizeof(test_source));

    client.data = &destination;
    client.write = write_test_buf;
    destination.len = LMQTT_TX_BUFFER_SIZE / 2;
    destination.available_len = destination.len;
    test_source.len = LMQTT_TX_BUFFER_SIZE / 4 * 5;
    test_source.available_len = test_source.len;
    for (i = 0; i < LMQTT_TX_BUFFER_SIZE / 4 * 5; i++)
        test_source.buf[i] = i % 199;

    process_output(&client);

    ck_assert_int_eq(LMQTT_TX_BUFFER_SIZE / 2, destination.pos);
    ck_assert_int_eq(2, destination.call_count);
    ck_assert_int_eq(LMQTT_TX_BUFFER_SIZE / 4 * 5, test_source.pos);
    ck_assert_int_eq(3, test_source.call_count);
    ck_assert_uint_eq(0 % 199, destination.buf[0]);
    ck_assert_uint_eq((LMQTT_TX_BUFFER_SIZE / 2 - 1) % 199,
        destination.buf[LMQTT_TX_BUFFER_SIZE / 2 - 1]);
}
END_TEST

START_TEST(should_fill_write_buffer_if_build_interrupts)
{
    lmqtt_client_t client;
    test_buffer_t destination;
    int i;

    memset(&client, 0, sizeof(client));
    memset(&destination, 0, sizeof(destination));
    memset(&test_source, 0, sizeof(test_source));

    client.data = &destination;
    client.write = write_test_buf;
    destination.len = LMQTT_TX_BUFFER_SIZE / 2;
    destination.available_len = destination.len;
    test_source.len = sizeof(test_source.buf);
    test_source.available_len = test_source.len;
    for (i = 0; i < sizeof(test_source.buf); i++)
        test_source.buf[i] = i % 199;

    process_output(&client);

    ck_assert_int_eq(LMQTT_TX_BUFFER_SIZE / 2, destination.pos);
    ck_assert_int_eq(2, destination.call_count);
    ck_assert_int_eq(LMQTT_TX_BUFFER_SIZE / 2 * 3, test_source.pos);
    ck_assert_int_eq(2, test_source.call_count);
    ck_assert_uint_eq(0 % 199, destination.buf[0]);
    ck_assert_uint_eq((LMQTT_TX_BUFFER_SIZE / 2 - 1) % 199,
        destination.buf[LMQTT_TX_BUFFER_SIZE / 2 - 1]);
}
END_TEST

START_TEST(should_process_remaining_output_from_previous_call)
{
    lmqtt_client_t client;
    test_buffer_t destination;
    int i;
    static int s = LMQTT_TX_BUFFER_SIZE / 8;

    memset(&client, 0, sizeof(client));
    memset(&destination, 0, sizeof(destination));
    memset(&test_source, 0, sizeof(test_source));

    client.data = &destination;
    client.write = write_test_buf;
    destination.len = s;
    destination.available_len = destination.len;
    test_source.len = sizeof(test_source.buf);
    test_source.available_len = test_source.len;
    for (i = 0; i < sizeof(test_source.buf); i++)
        test_source.buf[i] = i % 199;

    process_output(&client);

    ck_assert_int_eq(s, destination.pos);
    ck_assert_int_eq(2, destination.call_count);
    ck_assert_uint_eq(0 % 199, destination.buf[0]);
    ck_assert_uint_eq((s - 1) % 199, destination.buf[s - 1]);

    destination.pos = 0;
    process_output(&client);

    ck_assert_int_eq(s, destination.pos);
    ck_assert_int_eq(4, destination.call_count);
    ck_assert_uint_eq(s % 199, destination.buf[0]);
    ck_assert_uint_eq((s + s - 1) % 199, destination.buf[s - 1]);
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
