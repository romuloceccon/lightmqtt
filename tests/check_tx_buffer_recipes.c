#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

#define PREPARE \
    u8 buf[512]; \
    lmqtt_tx_buffer_t state; \
    lmqtt_io_result_t res; \
    int bytes_written; \
    memset(buf, 0xcc, sizeof(buf)); \
    memset(&state, 0xcc, sizeof(state))

START_TEST(should_encode_connect)
{
    u8 *expected_buffer;
    int i;
    lmqtt_connect_t connect;

    PREPARE;
    memset(&connect, 0, sizeof(connect));

    connect.keep_alive = 0x102;
    connect.client_id.buf = "a";
    connect.client_id.len = 1;
    connect.will_topic.buf = "b";
    connect.will_topic.len = 1;
    connect.will_message.buf = "c";
    connect.will_message.len = 1;
    connect.user_name.buf = "d";
    connect.user_name.len = 1;
    connect.password.buf = "e";
    connect.password.len = 1;

    lmqtt_tx_buffer_connect(&state, &connect);

    ck_assert_ptr_eq(recipe_connect, state.recipe);
    ck_assert_ptr_eq(&connect, state.recipe_data);

    ck_assert_int_eq(0, state.internal.recipe_pos);
    ck_assert_int_eq(0, state.internal.recipe_offset);

    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(27, bytes_written);

    expected_buffer = (u8 *) "\x10\x19\x00\x04" "MQTT\x04\xc4\x01\x02\x00\x01"
        "a\x00\x01" "b\x00\x01" "c\x00\x01" "d\x00\x01" "e";

    for (i = 0; i < 27; i++)
        ck_assert_int_eq(expected_buffer[i], buf[i]);
}
END_TEST

START_TEST(should_encode_pingreq)
{
    PREPARE;

    lmqtt_tx_buffer_pingreq(&state);

    ck_assert_ptr_eq(recipe_pingreq, state.recipe);
    ck_assert_ptr_eq(0, state.recipe_data);

    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(2, bytes_written);

    ck_assert_int_eq(0xc0, buf[0]);
    ck_assert_int_eq(0x00, buf[1]);
}
END_TEST

START_TEST(should_encode_disconnect)
{
    PREPARE;

    lmqtt_tx_buffer_disconnect(&state);

    ck_assert_ptr_eq(recipe_disconnect, state.recipe);
    ck_assert_ptr_eq(0, state.recipe_data);

    res = lmqtt_tx_buffer_encode(&state, buf, sizeof(buf), &bytes_written);

    ck_assert_int_eq(LMQTT_IO_SUCCESS, res);
    ck_assert_int_eq(2, bytes_written);

    ck_assert_int_eq(0xe0, buf[0]);
    ck_assert_int_eq(0x00, buf[1]);
}
END_TEST

START_TCASE("Tx buffer recipes")
{
    ADD_TEST(should_encode_connect);
    ADD_TEST(should_encode_pingreq);
    ADD_TEST(should_encode_disconnect);
}
END_TCASE
