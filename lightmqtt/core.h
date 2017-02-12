#ifndef _LIGHTMQTT_BASE_H_
#define _LIGHTMQTT_BASE_H_

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#ifdef HAVE_STDINT_H
    #include <stdint.h>
    typedef uint8_t u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
#else
    typedef unsigned char u8;
    typedef unsigned short u16;
    typedef unsigned long u32;
#endif

#define LMQTT_RX_BUFFER_SIZE 512
#define LMQTT_TX_BUFFER_SIZE 512

#define LMQTT_ENCODE_FINISHED 0
#define LMQTT_ENCODE_AGAIN 1
#define LMQTT_ENCODE_ERROR 2

#define LMQTT_DECODE_FINISHED 0
#define LMQTT_DECODE_AGAIN 1
#define LMQTT_DECODE_ERROR 2

#define LMQTT_ERR_FINISHED 0
#define LMQTT_ERR_AGAIN 1

typedef void (*LMqttCallback)(void *data);

typedef int (*LMqttEncodeFunction)(void *data, int offset, u8 *buf, int buf_len,
    int *bytes_written);

typedef int (*LMqttReadFunction)(void *, u8 *, int, int *);

typedef int (*LMqttWriteFunction)(void *, u8 *, int, int *);

typedef struct _LMqttTxBufferState {
    LMqttEncodeFunction *recipe;
    int recipe_pos;
    int recipe_offset;
    void *data;
} LMqttTxBufferState;

typedef struct _LMqttFixedHeader {
    int type;
    int dup;
    int qos;
    int retain;
    int remaining_length;
    int bytes_read;
    int failed;
    int remain_len_multiplier;
    int remain_len_accumulator;
    int remain_len_finished;
} LMqttFixedHeader;

typedef struct _LMqttConnack {
    int session_present;
    int return_code;
    int bytes_read;
    int failed;
} LMqttConnack;

typedef struct _LMqttCallbacks {
    int (*on_connack)(void *data, LMqttConnack *connack);
    int (*on_pingresp)(void *data);
} LMqttCallbacks;

typedef struct _LMqttRxBufferState {
    LMqttCallbacks *callbacks;
    void *callbacks_data;

    LMqttFixedHeader header;
    int header_finished;
    union {
        LMqttConnack connack;
    } payload;

    int failed;
} LMqttRxBufferState;

typedef struct _LMqttClient {
    void *data;
    LMqttReadFunction read;
    LMqttWriteFunction write;
    LMqttRxBufferState rx_state;
    LMqttTxBufferState tx_state;
    u8 read_buf[LMQTT_RX_BUFFER_SIZE];
    int read_buf_pos;
    u8 write_buf[LMQTT_TX_BUFFER_SIZE];
    int write_buf_pos;
} LMqttClient;

int encode_tx_buffer(LMqttTxBufferState *state, u8 *buf, int buf_len,
    int *bytes_written);

int decode_rx_buffer(LMqttRxBufferState *state, u8 *buf, int buf_len,
    int *bytes_read);

#endif
