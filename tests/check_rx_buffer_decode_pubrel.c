#include "check_lightmqtt.h"

static lmqtt_rx_buffer_t state;
static lmqtt_store_t store;
static int class;
static lmqtt_store_value_t value;

static void init_state()
{
    memset(&state, 0, sizeof(state));
    memset(&store, 0, sizeof(store));
    memset(&value, 0, sizeof(value));
    state.store = &store;
    store.get_time = &test_time_get;
    class = 0;
}

START_TEST(should_decode_pubrel_with_unknown_id)
{
    init_state();

    state.internal.packet_id = 0x0102;
    ck_assert_int_eq(1, rx_buffer_pubrel(&state));

    ck_assert_int_eq(1, lmqtt_store_peek(&store, &class, &value));
    ck_assert_int_eq(LMQTT_CLASS_PUBCOMP, class);
    ck_assert_int_eq(0x0102, value.packet_id);
}
END_TEST

START_TEST(should_decode_pubrel_with_existing_id)
{
    init_state();

    lmqtt_id_set_put(&state.id_set, 0x0102);

    state.internal.packet_id = 0x0102;
    ck_assert_int_eq(1, rx_buffer_pubrel(&state));

    lmqtt_store_peek(&store, &class, &value);
    ck_assert_int_eq(LMQTT_CLASS_PUBCOMP, class);

    ck_assert_int_eq(0, lmqtt_id_set_contains(&state.id_set, 0x0102));
}
END_TEST

START_TEST(should_not_decode_pubrel_with_full_store)
{
    int i;

    init_state();

    for (i = 0; i < LMQTT_STORE_SIZE; i++)
        lmqtt_store_append(&store, LMQTT_CLASS_PINGREQ, NULL);

    state.internal.packet_id = 0x0102;
    ck_assert_int_eq(0, rx_buffer_pubrel(&state));
}
END_TEST

START_TCASE("Rx buffer decode pubrel")
{
    ADD_TEST(should_decode_pubrel_with_unknown_id);
    ADD_TEST(should_decode_pubrel_with_existing_id);
    ADD_TEST(should_not_decode_pubrel_with_full_store);
}
END_TCASE
