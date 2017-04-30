#include "check_lightmqtt.h"

#define BYTES_R_PLACEHOLDER -12345

#define PREPARE \
    lmqtt_rx_buffer_t state; \
    lmqtt_store_t store; \
    u8 buf[64]; \
    int bytes_r = BYTES_R_PLACEHOLDER; \
    int res; \
    int data = 0; \
    lmqtt_store_value_t value; \
    memset(&client, 0, sizeof(client)); \
    memset(&buf, 0, sizeof(buf)); \
    memset(&state, 0, sizeof(state)); \
    memset(&store, 0, sizeof(store)); \
    memset(&value, 0, sizeof(value)); \
    state.store = &store; \
    store.get_time = &test_time_get; \
    value.value = &data

#define STORE_APPEND_MARK(class, id) \
    do { \
        value.packet_id = id; \
        ck_assert_int_eq(1, lmqtt_store_append(&store, class, &value)); \
        ck_assert_int_eq(1, lmqtt_store_mark_current(&store)); \
    } while (0)

typedef struct _test_packet_t {
    lmqtt_decode_result_t result;
    int bytes_to_read;
    int pos;
    u8 buf[256];
    u16 packet_id;
    void *packet_data;
} test_packet_t;

typedef struct _test_client_t {
    int current_packet;
    test_packet_t packets[10];
} test_client_t;

static test_client_t client;

/* mock */
int rx_buffer_call_callback_mock(lmqtt_rx_buffer_t *state)
{
    test_packet_t *packet = &client.packets[client.current_packet++];

    packet->packet_id = state->internal.packet_id;
    packet->packet_data = state->internal.value.value;

    return 1;
}

/* mock */
lmqtt_decode_result_t rx_buffer_decode_type_mock(
    lmqtt_rx_buffer_t *state, u8 b)
{
    test_packet_t *packet = &client.packets[client.current_packet];

    if (packet->pos >= packet->bytes_to_read)
        return LMQTT_DECODE_ERROR;

    packet->buf[packet->pos++] = b;
    return packet->pos >= packet->bytes_to_read ?
        packet->result : LMQTT_DECODE_CONTINUE;
}

void set_packet_result(int i, lmqtt_decode_result_t result, int bytes_to_read)
{
    test_packet_t *packet = &client.packets[i];
    packet->result = result;
    packet->bytes_to_read = bytes_to_read;
}

START_TEST(should_process_complete_rx_buffer)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[2] = 0;
    buf[3] = 5;

    STORE_APPEND_MARK(LMQTT_CLASS_CONNECT, 0);
    set_packet_result(0, LMQTT_DECODE_FINISHED, 2);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(4, bytes_r);

    ck_assert_int_eq(2, client.packets[0].pos);
    ck_assert_uint_eq(0, client.packets[0].buf[0]);
    ck_assert_uint_eq(5, client.packets[0].buf[1]);
    ck_assert_ptr_eq(&data, client.packets[0].packet_data);
}
END_TEST

START_TEST(should_process_partial_rx_buffer)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;

    STORE_APPEND_MARK(LMQTT_CLASS_CONNECT, 0);
    set_packet_result(0, LMQTT_DECODE_FINISHED, 2);

    res = lmqtt_rx_buffer_decode(&state, buf, 3, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(3, bytes_r);

    ck_assert_int_eq(1, client.packets[0].pos);
}
END_TEST

START_TEST(should_decode_rx_buffer_continuation)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;

    STORE_APPEND_MARK(LMQTT_CLASS_CONNECT, 0);
    set_packet_result(0, LMQTT_DECODE_FINISHED, 2);

    res = lmqtt_rx_buffer_decode(&state, buf, 1, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(1, bytes_r);
    ck_assert_int_eq(0, client.packets[0].pos);

    res = lmqtt_rx_buffer_decode(&state, buf + 1, 3, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(3, bytes_r);
    ck_assert_int_eq(2, client.packets[0].pos);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_invalid_header)
{
    PREPARE;

    buf[1] = 2;

    STORE_APPEND_MARK(LMQTT_CLASS_CONNECT, 0);
    set_packet_result(0, LMQTT_DECODE_FINISHED, 2);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(0, bytes_r);

    ck_assert_int_eq(0, client.packets[0].pos);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_invalid_data)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[2] = 0x0f;

    STORE_APPEND_MARK(LMQTT_CLASS_CONNECT, 0);
    set_packet_result(0, LMQTT_DECODE_ERROR, 1);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(2, bytes_r);

    ck_assert_int_eq(1, client.packets[0].pos);
}
END_TEST

START_TEST(should_not_decode_rx_buffer_after_error)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[2] = 0x0f;

    STORE_APPEND_MARK(LMQTT_CLASS_CONNECT, 0);
    set_packet_result(0, LMQTT_DECODE_ERROR, 1);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(2, bytes_r);

    buf[2] = 0;

    client.packets[0].result = LMQTT_DECODE_FINISHED;
    client.packets[0].bytes_to_read = 3;

    res = lmqtt_rx_buffer_decode(&state, buf + 2, 2, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(0, bytes_r);

    ck_assert_int_eq(1, client.packets[0].pos);
}
END_TEST

START_TEST(should_reset_rx_buffer_after_successful_processing)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;

    STORE_APPEND_MARK(LMQTT_CLASS_CONNECT, 0);
    STORE_APPEND_MARK(LMQTT_CLASS_CONNECT, 0);
    set_packet_result(0, LMQTT_DECODE_FINISHED, 2);
    set_packet_result(1, LMQTT_DECODE_FINISHED, 2);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(2, client.packets[0].pos);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(2, client.packets[0].pos);
    ck_assert_int_eq(2, client.packets[1].pos);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_two_packets)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[4] = 0x20;
    buf[5] = 2;

    STORE_APPEND_MARK(LMQTT_CLASS_CONNECT, 0);
    STORE_APPEND_MARK(LMQTT_CLASS_CONNECT, 0);
    set_packet_result(0, LMQTT_DECODE_FINISHED, 2);
    set_packet_result(1, LMQTT_DECODE_FINISHED, 2);

    res = lmqtt_rx_buffer_decode(&state, buf, 8, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(8, bytes_r);

    ck_assert_int_eq(2, client.packets[0].pos);
    ck_assert_int_eq(2, client.packets[1].pos);
}
END_TEST

START_TEST(should_touch_store_after_decode)
{
    int cnt = -1;
    long secs = -1, nsecs = -1;
    int tm_res;

    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[2] = 0;
    buf[3] = 5;

    store.timeout = 10;
    store.keep_alive = 20;

    STORE_APPEND_MARK(LMQTT_CLASS_CONNECT, 0);
    set_packet_result(0, LMQTT_DECODE_FINISHED, 2);

    test_time_set(8, 0);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);

    test_time_set(15, 0);

    tm_res = lmqtt_store_get_timeout(&store, &cnt, &secs, &nsecs);
    ck_assert_int_eq(1, tm_res);
    ck_assert_int_eq(0, cnt);
    ck_assert_int_eq(13, secs);
    ck_assert_int_eq(0, nsecs);
}
END_TEST

START_TEST(should_not_touch_store_after_decoding_empty_buffer)
{
    PREPARE;

    test_time_set(10, 0);

    res = lmqtt_rx_buffer_decode(&state, buf, 0, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);

    ck_assert_int_eq(0, store.last_touch.secs);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_allowed_null_data)
{
    PREPARE;

    buf[0] = 0xd0;
    buf[2] = 0xd0;
    buf[4] = 0xd0;

    STORE_APPEND_MARK(LMQTT_CLASS_PINGREQ, 0);
    STORE_APPEND_MARK(LMQTT_CLASS_PINGREQ, 0);
    STORE_APPEND_MARK(LMQTT_CLASS_PINGREQ, 0);

    res = lmqtt_rx_buffer_decode(&state, buf, 6, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(6, bytes_r);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_disallowed_null_data)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 0;
    buf[2] = 0xd0;

    STORE_APPEND_MARK(LMQTT_CLASS_CONNECT, 0);
    set_packet_result(0, LMQTT_DECODE_FINISHED, 2);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(2, bytes_r);

    ck_assert_int_eq(0, client.packets[0].pos);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_disallowed_nonnull_data)
{
    PREPARE;

    buf[0] = 0xd0;
    buf[1] = 1;

    STORE_APPEND_MARK(LMQTT_CLASS_PINGREQ, 0);
    set_packet_result(0, LMQTT_DECODE_ERROR, 0);

    res = lmqtt_rx_buffer_decode(&state, buf, 3, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(2, bytes_r);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_invalid_response_packet)
{
    PREPARE;

    buf[0] = 0xe0;

    STORE_APPEND_MARK(LMQTT_CLASS_DISCONNECT, 0);
    set_packet_result(0, LMQTT_DECODE_FINISHED, 2);

    res = lmqtt_rx_buffer_decode(&state, buf, 2, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(2, bytes_r);

    ck_assert_int_eq(0, client.packets[0].pos);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_packet_id)
{
    PREPARE;

    buf[0] = 0x40;
    buf[1] = 2;
    buf[2] = 0x01;
    buf[3] = 0x02;

    STORE_APPEND_MARK(LMQTT_CLASS_PUBLISH_1, 0x0102);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(4, bytes_r);

    ck_assert_int_eq(0x0102, client.packets[0].packet_id);
}
END_TEST

START_TEST(should_decode_rx_buffer_after_packet_id)
{
    PREPARE;

    buf[0] = 0x90;
    buf[1] = 3;
    buf[2] = 0x03;
    buf[3] = 0x04;

    STORE_APPEND_MARK(LMQTT_CLASS_SUBSCRIBE, 0x0304);
    set_packet_result(0, LMQTT_DECODE_FINISHED, 1);

    res = lmqtt_rx_buffer_decode(&state, buf, 5, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(5, bytes_r);

    ck_assert_int_eq(0x0304, client.packets[0].packet_id);
    ck_assert_int_eq(1, client.packets[0].pos);
}
END_TEST

START_TEST(should_decode_pubrec)
{
    int store_class;
    lmqtt_store_value_t store_value;

    PREPARE;

    buf[0] = 0x50;
    buf[1] = 2;
    buf[2] = 0x00;
    buf[3] = 0x03;

    STORE_APPEND_MARK(LMQTT_CLASS_PUBLISH_2, 3);
    set_packet_result(0, LMQTT_IO_SUCCESS, 4);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);

    ck_assert_int_eq(0, client.current_packet);
    ck_assert_int_eq(1, lmqtt_store_shift(&store, &store_class, &store_value));
    ck_assert_int_eq(LMQTT_CLASS_PUBREL, store_class);
    ck_assert_ptr_eq(&data, store_value.value);
}
END_TEST

START_TEST(should_fail_if_decoder_does_not_finish_when_expected)
{
    PREPARE;

    buf[0] = 0x90;
    buf[1] = 3;
    buf[2] = 0x01;
    buf[3] = 0x02;

    STORE_APPEND_MARK(LMQTT_CLASS_SUBSCRIBE, 0x0102);
    set_packet_result(0, LMQTT_DECODE_FINISHED, 2);

    res = lmqtt_rx_buffer_decode(&state, buf, 5, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(4, bytes_r);

    ck_assert_int_eq(0, client.packets[0].packet_id);
    ck_assert_int_eq(1, client.packets[0].pos);
}
END_TEST

START_TEST(should_fail_if_decoder_finishes_before_expected)
{
    PREPARE;

    buf[0] = 0x90;
    buf[1] = 4;
    buf[2] = 0x01;
    buf[3] = 0x02;

    STORE_APPEND_MARK(LMQTT_CLASS_SUBSCRIBE, 0x0102);
    set_packet_result(0, LMQTT_DECODE_FINISHED, 1);

    res = lmqtt_rx_buffer_decode(&state, buf, 6, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(4, bytes_r);

    ck_assert_int_eq(0, client.packets[0].packet_id);
    ck_assert_int_eq(1, client.packets[0].pos);
}
END_TEST

START_TEST(should_fail_if_connack_has_no_corresponding_connect)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[2] = 0;
    buf[3] = 5;

    set_packet_result(0, LMQTT_DECODE_FINISHED, 2);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(2, bytes_r);
}
END_TEST

START_TEST(should_fail_if_suback_has_no_corresponding_subscribe)
{
    PREPARE;

    buf[0] = 0x90;
    buf[1] = 4;
    buf[2] = 0x01;
    buf[3] = 0x02;

    STORE_APPEND_MARK(LMQTT_CLASS_SUBSCRIBE, 0x0111);
    set_packet_result(0, LMQTT_DECODE_FINISHED, 2);

    res = lmqtt_rx_buffer_decode(&state, buf, 6, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(3, bytes_r);
}
END_TEST

START_TEST(should_decode_pubrel_without_corresponding_pubrec)
{
    PREPARE;

    buf[0] = 0x62;
    buf[1] = 2;
    buf[2] = 0x01;
    buf[3] = 0x02;

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(4, bytes_r);

    ck_assert_int_eq(0x0102, client.packets[0].packet_id);
}
END_TEST

START_TEST(should_finish_failed_buffer)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[2] = 0x0f;

    STORE_APPEND_MARK(LMQTT_CLASS_CONNECT, 0);
    set_packet_result(0, LMQTT_DECODE_ERROR, 1);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_ERROR, res);

    lmqtt_rx_buffer_finish(&state);
    ck_assert_int_eq(1, client.current_packet);
    ck_assert_ptr_eq(&data, client.packets[0].packet_data);
}
END_TEST

START_TCASE("Rx buffer decode")
{
    rx_buffer_call_callback = &rx_buffer_call_callback_mock;
    rx_buffer_decode_type = &rx_buffer_decode_type_mock;

    ADD_TEST(should_process_complete_rx_buffer);
    ADD_TEST(should_process_partial_rx_buffer);
    ADD_TEST(should_decode_rx_buffer_continuation);
    ADD_TEST(should_decode_rx_buffer_with_invalid_header);
    ADD_TEST(should_decode_rx_buffer_with_invalid_data);
    ADD_TEST(should_not_decode_rx_buffer_after_error);
    ADD_TEST(should_reset_rx_buffer_after_successful_processing);
    ADD_TEST(should_decode_rx_buffer_with_two_packets);
    ADD_TEST(should_touch_store_after_decode);
    ADD_TEST(should_not_touch_store_after_decoding_empty_buffer);
    ADD_TEST(should_decode_rx_buffer_with_allowed_null_data);
    ADD_TEST(should_decode_rx_buffer_with_disallowed_null_data);
    ADD_TEST(should_decode_rx_buffer_with_disallowed_nonnull_data);
    ADD_TEST(should_decode_rx_buffer_with_invalid_response_packet);
    ADD_TEST(should_decode_rx_buffer_with_packet_id);
    ADD_TEST(should_decode_rx_buffer_after_packet_id);
    ADD_TEST(should_decode_pubrec);
    ADD_TEST(should_fail_if_decoder_does_not_finish_when_expected);
    ADD_TEST(should_fail_if_decoder_finishes_before_expected);
    ADD_TEST(should_fail_if_connack_has_no_corresponding_connect);
    ADD_TEST(should_fail_if_suback_has_no_corresponding_subscribe);
    ADD_TEST(should_decode_pubrel_without_corresponding_pubrec);
    ADD_TEST(should_finish_failed_buffer);
}
END_TCASE
