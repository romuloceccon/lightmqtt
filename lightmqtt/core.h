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

typedef int (*lmqtt_encode_t)(void *data, int offset, u8 *buf, int buf_len,
    int *bytes_written);

typedef int (*lmqtt_read_t)(void *, u8 *, int, int *);

typedef int (*lmqtt_write_t)(void *, u8 *, int, int *);

typedef struct _lmqtt_tx_buffer_state_t {
    lmqtt_encode_t *recipe;
    int recipe_pos;
    int recipe_offset;
    void *data;
} lmqtt_tx_buffer_state_t;

typedef struct _lmqtt_fixed_header_t {
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
} lmqtt_fixed_header_t;

typedef struct _lmqtt_connack_t {
    int session_present;
    int return_code;
    int bytes_read;
    int failed;
} lmqtt_connack_t;

typedef struct _lmqtt_callbacks_t {
    int (*on_connack)(void *data, lmqtt_connack_t *connack);
    int (*on_pingresp)(void *data);
} lmqtt_callbacks_t;

typedef struct _lmqtt_rx_buffer_state_t {
    lmqtt_callbacks_t *callbacks;
    void *callbacks_data;

    lmqtt_fixed_header_t header;
    int header_finished;
    union {
        lmqtt_connack_t connack;
    } payload;

    int failed;
} lmqtt_rx_buffer_state_t;

typedef struct _lmqtt_client_t {
    void *data;
    lmqtt_read_t read;
    lmqtt_write_t write;
    lmqtt_rx_buffer_state_t rx_state;
    lmqtt_tx_buffer_state_t tx_state;
    u8 read_buf[LMQTT_RX_BUFFER_SIZE];
    int read_buf_pos;
    u8 write_buf[LMQTT_TX_BUFFER_SIZE];
    int write_buf_pos;
} lmqtt_client_t;

int encode_tx_buffer(lmqtt_tx_buffer_state_t *state, u8 *buf, int buf_len,
    int *bytes_written);

int decode_rx_buffer(lmqtt_rx_buffer_state_t *state, u8 *buf, int buf_len,
    int *bytes_read);

#endif
