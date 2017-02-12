#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

#define BYTES_R_PLACEHOLDER -12345

#define PREPARE \
    static TestClient client; \
    lmqtt_rx_buffer_t state; \
    u8 buf[64]; \
    int bytes_r = BYTES_R_PLACEHOLDER; \
    int res; \
    memset(&client, 0, sizeof(client)); \
    memset(&buf, 0, sizeof(buf)); \
    memset(&state, 0, sizeof(state)); \
    state.callbacks = &callbacks; \
    state.callbacks_data = &client

typedef struct _TestClient {
    int type;
    int param;
    int count;
} TestClient;

static int on_connack(void *data, lmqtt_connack_t *connack)
{
    TestClient *client = (TestClient *) data;
    client->count += 1;
    client->type = LMQTT_TYPE_CONNACK;
    client->param = connack->return_code;
    return 0;
}

static int on_pingresp(void *data)
{
    TestClient *client = (TestClient *) data;
    client->count += 1;
    client->type = LMQTT_TYPE_PINGRESP;
    client->param = 0;
    return 0;
}

static lmqtt_callbacks_t callbacks = { on_connack, on_pingresp };

START_TEST(should_process_complete_rx_buffer)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[2] = 0;
    buf[3] = 5;

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(4, bytes_r);

    ck_assert_int_eq(1, client.count);
    ck_assert_int_eq(LMQTT_TYPE_CONNACK, client.type);
    ck_assert_int_eq(5, client.param);
}
END_TEST

START_TEST(should_process_partial_rx_buffer)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;

    res = lmqtt_rx_buffer_decode(&state, buf, 3, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(3, bytes_r);

    ck_assert_int_eq(0, client.count);
}
END_TEST

START_TEST(should_decode_rx_buffer_continuation)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;

    res = lmqtt_rx_buffer_decode(&state, buf, 1, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(1, bytes_r);
    ck_assert_int_eq(0, client.count);

    res = lmqtt_rx_buffer_decode(&state, buf + 1, 3, &bytes_r);
    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(3, bytes_r);
    ck_assert_int_eq(1, client.count);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_invalid_header)
{
    PREPARE;

    buf[1] = 2;

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(1, bytes_r);

    ck_assert_int_eq(0, client.count);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_invalid_data)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[2] = 0x0f;

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(3, bytes_r);

    ck_assert_int_eq(0, client.count);
}
END_TEST

START_TEST(should_not_decode_rx_buffer_after_error)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[2] = 0x0f;

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(3, bytes_r);

    buf[2] = 0;

    res = lmqtt_rx_buffer_decode(&state, buf + 2, 2, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(0, bytes_r);
}
END_TEST

START_TEST(should_reset_rx_buffer_after_successful_processing)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(1, client.count);

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(2, client.count);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_two_packets)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 2;
    buf[4] = 0x20;
    buf[5] = 2;

    res = lmqtt_rx_buffer_decode(&state, buf, 8, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(8, bytes_r);

    ck_assert_int_eq(2, client.count);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_allowed_null_data)
{
    PREPARE;

    buf[0] = 0xd0;
    buf[2] = 0xd0;
    buf[4] = 0xd0;

    res = lmqtt_rx_buffer_decode(&state, buf, 6, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(6, bytes_r);

    ck_assert_int_eq(3, client.count);
    ck_assert_int_eq(LMQTT_TYPE_PINGRESP, client.type);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_disallowed_null_data)
{
    PREPARE;

    buf[0] = 0x20;
    buf[1] = 0;
    buf[2] = 0xd0;

    res = lmqtt_rx_buffer_decode(&state, buf, 4, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(2, bytes_r);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_disallowed_nonnull_data)
{
    PREPARE;

    buf[0] = 0xd0;
    buf[1] = 1;

    res = lmqtt_rx_buffer_decode(&state, buf, 3, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(2, bytes_r);
}
END_TEST

START_TEST(should_decode_rx_buffer_with_invalid_response_packet)
{
    PREPARE;

    buf[0] = 0xe0;

    res = lmqtt_rx_buffer_decode(&state, buf, 2, &bytes_r);

    ck_assert_int_eq(LMQTT_IO_ERROR, res);
    ck_assert_int_eq(2, bytes_r);
}
END_TEST

START_TCASE("Process rx buffer")
{
    ADD_TEST(should_process_complete_rx_buffer);
    ADD_TEST(should_process_partial_rx_buffer);
    ADD_TEST(should_decode_rx_buffer_continuation);
    ADD_TEST(should_decode_rx_buffer_with_invalid_header);
    ADD_TEST(should_decode_rx_buffer_with_invalid_data);
    ADD_TEST(should_not_decode_rx_buffer_after_error);
    ADD_TEST(should_reset_rx_buffer_after_successful_processing);
    ADD_TEST(should_decode_rx_buffer_with_two_packets);
    ADD_TEST(should_decode_rx_buffer_with_allowed_null_data);
    ADD_TEST(should_decode_rx_buffer_with_disallowed_null_data);
    ADD_TEST(should_decode_rx_buffer_with_disallowed_nonnull_data);
    ADD_TEST(should_decode_rx_buffer_with_invalid_response_packet);
}
END_TCASE
