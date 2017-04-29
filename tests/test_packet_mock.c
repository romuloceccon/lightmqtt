#define LMQTT_TEST

#include "lightmqtt/packet.h"

#define TX_BUFFER_FINDER_BY_CLASS tx_buffer_finder_by_class_mock
#define RX_BUFFER_CALL_CALLBACK rx_buffer_call_callback_mock
#define RX_BUFFER_DECODE_TYPE rx_buffer_decode_type_mock

lmqtt_encoder_finder_t tx_buffer_finder_by_class_mock(lmqtt_class_t class);

int rx_buffer_call_callback_mock(lmqtt_rx_buffer_t *state);

lmqtt_decode_result_t rx_buffer_decode_type_mock(
    lmqtt_rx_buffer_t *state, u8 b);

#include "../src/lmqtt_packet.c"
