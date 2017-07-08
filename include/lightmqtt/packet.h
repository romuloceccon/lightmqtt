#ifndef _LIGHTMQTT_PACKET_H_
#define _LIGHTMQTT_PACKET_H_

#include <stddef.h>
#include <lightmqtt/core.h>
#include <lightmqtt/store.h>

#define LMQTT_CONNACK_RC_ACCEPTED 0
#define LMQTT_CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION 1
#define LMQTT_CONNACK_RC_IDENTIFIER_REJECTED 2
#define LMQTT_CONNACK_RC_SERVER_UNAVAILABLE 3
#define LMQTT_CONNACK_RC_BAD_USER_NAME_OR_PASSWORD 4
#define LMQTT_CONNACK_RC_NOT_AUTHORIZED 5
#define LMQTT_CONNACK_RC_MAX 5

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum {
    LMQTT_KIND_CONNECT = 200,
    LMQTT_KIND_PUBLISH_0,
    LMQTT_KIND_PUBLISH_1,
    LMQTT_KIND_PUBLISH_2,
    LMQTT_KIND_PUBACK,
    LMQTT_KIND_PUBREC,
    LMQTT_KIND_PUBREL,
    LMQTT_KIND_PUBCOMP,
    LMQTT_KIND_SUBSCRIBE,
    LMQTT_KIND_UNSUBSCRIBE,
    LMQTT_KIND_PINGREQ,
    LMQTT_KIND_DISCONNECT
} lmqtt_kind_t;

typedef enum {
    LMQTT_QOS_0 = 0,
    LMQTT_QOS_1,
    LMQTT_QOS_2
} lmqtt_qos_t;

typedef enum {
    LMQTT_ENCODE_FINISHED = 110,
    LMQTT_ENCODE_CONTINUE,
    LMQTT_ENCODE_WOULD_BLOCK,
    LMQTT_ENCODE_ERROR
} lmqtt_encode_result_t;

typedef enum {
    LMQTT_DECODE_FINISHED = 120,
    LMQTT_DECODE_CONTINUE,
    LMQTT_DECODE_WOULD_BLOCK,
    LMQTT_DECODE_ERROR
} lmqtt_decode_result_t;

typedef enum {
    LMQTT_ALLOCATE_SUCCESS = 130,
    LMQTT_ALLOCATE_IGNORE,
    LMQTT_ALLOCATE_ERROR
} lmqtt_allocate_result_t;

typedef struct _lmqtt_id_set_t {
    lmqtt_packet_id_t *items;
    size_t capacity;
    size_t count;
} lmqtt_id_set_t;

typedef struct _lmqtt_string_t {
    long len;
    char *buf;
    void *data;
    lmqtt_read_t read;
    lmqtt_write_t write;
    struct {
        long pos;
    } internal;
} lmqtt_string_t;

typedef struct _lmqtt_encode_buffer_t {
    int encoded;
    size_t buf_len;
    unsigned char buf[16];
    lmqtt_string_t *blocking_str;
} lmqtt_encode_buffer_t;

typedef lmqtt_encode_result_t (*encode_buffer_builder_t)(lmqtt_store_value_t *,
    lmqtt_encode_buffer_t *);

typedef struct _lmqtt_fixed_header_t {
    unsigned char type;
    unsigned char dup;
    unsigned char qos;
    unsigned char retain;
    long remaining_length;
    struct {
        size_t bytes_read;
        int failed;
        long remain_len_multiplier;
        long remain_len_accumulator;
        long remain_len_finished;
    } internal;
} lmqtt_fixed_header_t;

typedef struct _lmqtt_subscription_t {
    lmqtt_qos_t requested_qos;
    lmqtt_string_t topic;
    unsigned char return_code;
} lmqtt_subscription_t;

typedef struct _lmqtt_connect_t {
    unsigned short keep_alive;
    unsigned char clean_session;
    lmqtt_qos_t will_qos;
    unsigned char will_retain;
    lmqtt_string_t client_id;
    lmqtt_string_t will_topic;
    lmqtt_string_t will_message;
    lmqtt_string_t user_name;
    lmqtt_string_t password;
    struct {
        unsigned char session_present;
        unsigned char return_code;
    } response;
} lmqtt_connect_t;

typedef struct _lmqtt_subscribe_t {
    int count;
    lmqtt_subscription_t *subscriptions;
    struct {
        lmqtt_subscription_t *current;
    } internal;
} lmqtt_subscribe_t;

typedef struct _lmqtt_publish_t {
    lmqtt_qos_t qos;
    unsigned char retain;
    lmqtt_string_t topic;
    lmqtt_string_t payload;
    struct {
        int encode_count;
    } internal;
} lmqtt_publish_t;

typedef struct _lmqtt_tx_buffer_t {
    lmqtt_store_t *store;

    int closed;

    struct {
        int pos;
        size_t offset;
        lmqtt_encode_buffer_t buffer;
        int failed;
    } internal;
} lmqtt_tx_buffer_t;

typedef lmqtt_encode_result_t (*lmqtt_encoder_t)(lmqtt_store_value_t *,
    lmqtt_encode_buffer_t *, size_t, unsigned char *, size_t, size_t *);

typedef lmqtt_encoder_t (*lmqtt_encoder_finder_t)(lmqtt_tx_buffer_t *,
    lmqtt_store_value_t *);

typedef struct _lmqtt_decode_bytes_t {
    size_t buf_len;
    unsigned char *buf;
    size_t *bytes_written;
} lmqtt_decode_bytes_t;

struct _lmqtt_rx_buffer_t;

struct _lmqtt_rx_buffer_decoder_t {
    long min_length;
    lmqtt_kind_t kind;
    int (*pop_packet_without_id)(struct _lmqtt_rx_buffer_t *);
    int (*pop_packet_with_id)(struct _lmqtt_rx_buffer_t *);
    lmqtt_decode_result_t (*decode_remaining)(struct _lmqtt_rx_buffer_t *,
        lmqtt_decode_bytes_t *);
    lmqtt_decode_result_t (*decode_bytes)(struct _lmqtt_rx_buffer_t *,
        lmqtt_decode_bytes_t *);
};

typedef int (*lmqtt_message_on_publish_t)(void *, lmqtt_publish_t *);
typedef lmqtt_allocate_result_t (*lmqtt_message_on_publish_allocate_t)(void *,
    lmqtt_publish_t *, size_t);
typedef void (*lmqtt_message_on_publish_deallocate_t)(void *,
    lmqtt_publish_t *);

typedef struct _lmqtt_message_callbacks_t {
    lmqtt_message_on_publish_t on_publish;
    lmqtt_message_on_publish_allocate_t on_publish_allocate_topic;
    lmqtt_message_on_publish_allocate_t on_publish_allocate_payload;
    lmqtt_message_on_publish_deallocate_t on_publish_deallocate;
    void *on_publish_data;
} lmqtt_message_callbacks_t;

typedef struct _lmqtt_rx_buffer_t {
    lmqtt_store_t *store;
    lmqtt_message_callbacks_t *message_callbacks;

    lmqtt_id_set_t id_set;

    struct {
        lmqtt_fixed_header_t header;
        int header_finished;
        struct _lmqtt_rx_buffer_decoder_t const *decoder;
        long remain_buf_pos;
        unsigned short topic_len;
        lmqtt_packet_id_t packet_id;
        lmqtt_store_value_t value;
        lmqtt_publish_t publish;
        int ignore_publish;
        lmqtt_string_t *blocking_str;
        int failed;
    } internal;
} lmqtt_rx_buffer_t;

int lmqtt_id_set_clear(lmqtt_id_set_t *id_set);
int lmqtt_id_set_contains(lmqtt_id_set_t *id_set, lmqtt_packet_id_t id);
int lmqtt_id_set_put(lmqtt_id_set_t *id_set, lmqtt_packet_id_t id);
int lmqtt_id_set_remove(lmqtt_id_set_t *id_set, lmqtt_packet_id_t id);

int lmqtt_connect_validate(lmqtt_connect_t *connect);
int lmqtt_subscribe_validate(lmqtt_subscribe_t *subscribe);
int lmqtt_publish_validate(lmqtt_publish_t *publish);

void lmqtt_tx_buffer_reset(lmqtt_tx_buffer_t *state);
void lmqtt_tx_buffer_finish(lmqtt_tx_buffer_t *state);
lmqtt_string_t *lmqtt_tx_buffer_get_blocking_str(lmqtt_tx_buffer_t *state);
extern lmqtt_io_result_t (*lmqtt_tx_buffer_encode)(lmqtt_tx_buffer_t *state,
    unsigned char *buf, size_t buf_len, size_t *bytes_written);

void lmqtt_rx_buffer_reset(lmqtt_rx_buffer_t *state);
void lmqtt_rx_buffer_finish(lmqtt_rx_buffer_t *state);
lmqtt_string_t *lmqtt_rx_buffer_get_blocking_str(lmqtt_rx_buffer_t *state);
extern lmqtt_io_result_t (*lmqtt_rx_buffer_decode)(lmqtt_rx_buffer_t *state,
    unsigned char *buf, size_t buf_len, size_t *bytes_read);

#ifdef  __cplusplus
}
#endif

#endif
