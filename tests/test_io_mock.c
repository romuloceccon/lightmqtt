#define LMQTT_TEST

#include "lightmqtt/packet.h"

#define LMQTT_RX_BUFFER_DECODE lmqtt_rx_buffer_decode_mock
#define LMQTT_TX_BUFFER_ENCODE lmqtt_tx_buffer_encode_mock

lmqtt_io_result_t lmqtt_rx_buffer_decode_mock(lmqtt_rx_buffer_t *state, u8 *buf,
    int buf_len, int *bytes_read);
lmqtt_io_result_t lmqtt_tx_buffer_encode_mock(lmqtt_tx_buffer_t *state, u8 *buf,
    int buf_len, int *bytes_written);

#include "../src/lmqtt_io.c"
