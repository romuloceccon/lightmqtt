#ifndef _LIGHTMQTT_PACKET_H_
#define _LIGHTMQTT_PACKET_H_

#include <lightmqtt/core.h>
#include <lightmqtt/store.h>

#define LMQTT_FIXED_HEADER_MAX_SIZE 5
#define LMQTT_CONNECT_HEADER_SIZE 10
#define LMQTT_CONNACK_HEADER_SIZE 2

#define LMQTT_CONNACK_RC_ACCEPTED 0
#define LMQTT_CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION 1
#define LMQTT_CONNACK_RC_IDENTIFIER_REJECTED 2
#define LMQTT_CONNACK_RC_SERVER_UNAVAILABLE 3
#define LMQTT_CONNACK_RC_BAD_USER_NAME_OR_PASSWORD 4
#define LMQTT_CONNACK_RC_NOT_AUTHORIZED 5
#define LMQTT_CONNACK_RC_MAX 5

typedef enum {
    LMQTT_ENCODE_FINISHED = 110,
    LMQTT_ENCODE_CONTINUE,
    LMQTT_ENCODE_WOULD_BLOCK,
    LMQTT_ENCODE_ERROR
} lmqtt_encode_result_t;

typedef enum {
    LMQTT_DECODE_FINISHED = 120,
    LMQTT_DECODE_CONTINUE,
    LMQTT_DECODE_ERROR
} lmqtt_decode_result_t;

typedef enum {
    LMQTT_READ_SUCCESS = 130,
    LMQTT_READ_WOULD_BLOCK,
    LMQTT_READ_ERROR
} lmqtt_read_result_t;

typedef enum {
    LMQTT_OPERATION_PUBLISH = 140,
    LMQTT_OPERATION_SUBSCRIBE,
    LMQTT_OPERATION_UNSUBSCRIBE
} lmqtt_operation_t;

typedef struct _lmqtt_encode_buffer_t {
    int encoded;
    int buf_len;
    u8 buf[16];
} lmqtt_encode_buffer_t;

typedef lmqtt_encode_result_t (*encode_buffer_builder_t)(void *,
    lmqtt_encode_buffer_t *);

typedef struct _lmqtt_string_t {
    int len;
    char *buf;
    void *data;
    lmqtt_read_result_t (*read)(void *, u8 *, int, int *);
} lmqtt_string_t;

/* TODO: review this; only remaining_length is actually part of the fixed
 * header */
typedef struct _lmqtt_fixed_header_t {
    int type;
    int dup;
    int qos;
    int retain;
    int remaining_length;
    struct {
        int bytes_read;
        int failed;
        int remain_len_multiplier;
        int remain_len_accumulator;
        int remain_len_finished;
    } internal;
} lmqtt_fixed_header_t;

typedef struct _lmqtt_subscription_t {
    int qos;
    lmqtt_string_t topic;
    int return_code;
} lmqtt_subscription_t;

typedef struct _lmqtt_connect_t {
    u16 keep_alive;
    int clean_session;
    int qos;
    int will_retain;
    lmqtt_string_t client_id;
    lmqtt_string_t will_topic;
    lmqtt_string_t will_message;
    lmqtt_string_t user_name;
    lmqtt_string_t password;
    struct {
        int session_present;
        int return_code;
    } response;
} lmqtt_connect_t;

typedef struct _lmqtt_subscribe_t {
    u16 packet_id;
    int count;
    lmqtt_subscription_t *subscriptions;
    struct {
        lmqtt_subscription_t *current;
    } internal;
} lmqtt_subscribe_t;

typedef lmqtt_encode_result_t (*lmqtt_encoder_t)(void *,
    lmqtt_encode_buffer_t *, int, u8 *, int, int *);

typedef struct _lmqtt_tx_buffer_t {
    lmqtt_store_t *store;

    struct {
        int pos;
        int offset;
        lmqtt_encode_buffer_t buffer;
    } internal;
} lmqtt_tx_buffer_t;

typedef lmqtt_encoder_t (*lmqtt_encoder_finder_t)(lmqtt_tx_buffer_t *, void *);

typedef struct _lmqtt_callbacks_t {
    int (*on_connack)(void *, lmqtt_connect_t *);
    int (*on_suback)(void *, lmqtt_subscribe_t *);
    int (*on_pingresp)(void *);
} lmqtt_callbacks_t;

struct _lmqtt_rx_buffer_t;

struct _lmqtt_rx_buffer_decoder_t {
    int min_length;
    lmqtt_class_t class;
    int (*pop_packet_without_id)(struct _lmqtt_rx_buffer_t *);
    int (*pop_packet_with_id)(struct _lmqtt_rx_buffer_t *);
    int (*decode_remaining)(struct _lmqtt_rx_buffer_t *, u8);
    lmqtt_decode_result_t (*decode_byte)(struct _lmqtt_rx_buffer_t *, u8);
    int (*call_callback)(struct _lmqtt_rx_buffer_t *);
};

typedef struct _lmqtt_rx_buffer_t {
    lmqtt_store_t *store;

    lmqtt_callbacks_t *callbacks;
    void *callbacks_data;

    struct {
        lmqtt_fixed_header_t header;
        int header_finished;
        struct _lmqtt_rx_buffer_decoder_t const *decoder;
        int remain_buf_pos;
        u16 packet_id;
        void *packet_data;
        int failed;
    } internal;
} lmqtt_rx_buffer_t;

lmqtt_io_result_t lmqtt_tx_buffer_encode(lmqtt_tx_buffer_t *state, u8 *buf,
    int buf_len, int *bytes_written);

lmqtt_io_result_t lmqtt_rx_buffer_decode(lmqtt_rx_buffer_t *state, u8 *buf,
    int buf_len, int *bytes_read);

#endif
