#ifndef _LIGHTMQTT_IO_H_
#define _LIGHTMQTT_IO_H_

#include <lightmqtt/packet.h>

#define LMQTT_RX_BUFFER_SIZE 512
#define LMQTT_TX_BUFFER_SIZE 512

typedef int (*lmqtt_read_t)(void *, u8 *, int, int *);

typedef int (*lmqtt_write_t)(void *, u8 *, int, int *);

typedef struct _lmqtt_client_t {
    void *data;
    lmqtt_read_t read;
    lmqtt_write_t write;
    lmqtt_rx_buffer_t rx_state;
    lmqtt_tx_buffer_t tx_state;
    u8 read_buf[LMQTT_RX_BUFFER_SIZE];
    int read_buf_pos;
    u8 write_buf[LMQTT_TX_BUFFER_SIZE];
    int write_buf_pos;
} lmqtt_client_t;

#endif
