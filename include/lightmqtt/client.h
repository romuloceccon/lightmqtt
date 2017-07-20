#ifndef _LIGHTMQTT_CLIENT_H_
#define _LIGHTMQTT_CLIENT_H_

#include <lightmqtt/time.h>
#include <lightmqtt/packet.h>

#define LMQTT_RES_ERROR               0x00ff
#define LMQTT_RES_WOULD_BLOCK_CONN_RD 0x0100
#define LMQTT_RES_WOULD_BLOCK_CONN_WR 0x0200
#define LMQTT_RES_WOULD_BLOCK_DATA_RD 0x0400
#define LMQTT_RES_WOULD_BLOCK_DATA_WR 0x0800
#define LMQTT_RES_EOF                 0x1000
#define LMQTT_RES_QUEUEABLE           0x2000

#define LMQTT_RES_EOF_RD (LMQTT_RES_EOF | LMQTT_RES_WOULD_BLOCK_CONN_RD)
#define LMQTT_RES_EOF_WR (LMQTT_RES_EOF | LMQTT_RES_WOULD_BLOCK_CONN_WR)

#define LMQTT_IS_ERROR(res) \
    (((res) & LMQTT_RES_ERROR) != 0)
#define LMQTT_WOULD_BLOCK_CONN_RD(res) \
    (((res) & LMQTT_RES_EOF_RD) == LMQTT_RES_WOULD_BLOCK_CONN_RD)
#define LMQTT_WOULD_BLOCK_CONN_WR(res) \
    (((res) & LMQTT_RES_EOF_WR) == LMQTT_RES_WOULD_BLOCK_CONN_WR)
#define LMQTT_WOULD_BLOCK_DATA_RD(res) \
    (((res) & LMQTT_RES_WOULD_BLOCK_DATA_RD) != 0)
#define LMQTT_WOULD_BLOCK_DATA_WR(res) \
    (((res) & LMQTT_RES_WOULD_BLOCK_DATA_WR) != 0)
#define LMQTT_IS_EOF(res) \
    (((res) & LMQTT_RES_EOF) != 0)
#define LMQTT_IS_EOF_RD(res) \
    (((res) & LMQTT_RES_EOF_RD) == LMQTT_RES_EOF_RD)
#define LMQTT_IS_EOF_WR(res) \
    (((res) & LMQTT_RES_EOF_WR) == LMQTT_RES_EOF_WR)
#define LMQTT_IS_QUEUEABLE(res) \
    (((res) & LMQTT_RES_QUEUEABLE) != 0)
#define LMQTT_ERROR_NUM(res) (0)

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct _lmqtt_client_callbacks_t {
    void *data;
    lmqtt_io_callback_t read;
    lmqtt_io_callback_t write;
    lmqtt_get_time_t get_time;
} lmqtt_client_callbacks_t;

typedef struct _lmqtt_client_buffers_t {
    size_t store_size;
    void *store;
    size_t rx_buffer_size;
    void *rx_buffer;
    size_t tx_buffer_size;
    void *tx_buffer;
    size_t id_set_size;
    void *id_set;
} lmqtt_client_buffers_t;

typedef void (*lmqtt_client_on_connect_t)(void *, lmqtt_connect_t *, int);
typedef void (*lmqtt_client_on_subscribe_t)(void *, lmqtt_subscribe_t *, int);
typedef void (*lmqtt_client_on_unsubscribe_t)(void *, lmqtt_subscribe_t *, int);
typedef void (*lmqtt_client_on_publish_t)(void *, lmqtt_publish_t *, int);

struct _lmqtt_client_t;

typedef struct _lmqtt_client_t {
    lmqtt_client_on_connect_t on_connect;
    void *on_connect_data;
    lmqtt_client_on_subscribe_t on_subscribe;
    void *on_subscribe_data;
    lmqtt_client_on_unsubscribe_t on_unsubscribe;
    void *on_unsubscribe_data;
    lmqtt_client_on_publish_t on_publish;
    void *on_publish_data;

    int closed;
    int failed;
    int clean_session;

    lmqtt_rx_buffer_t rx_state;
    lmqtt_tx_buffer_t tx_state;
    unsigned char *read_buf;
    size_t read_buf_pos;
    size_t read_buf_capacity;
    unsigned char *write_buf;
    size_t write_buf_pos;
    size_t write_buf_capacity;
    lmqtt_store_t main_store;
    lmqtt_store_t connect_store;
    lmqtt_store_t *current_store;
    lmqtt_store_entry_t connect_store_entry;

    lmqtt_client_callbacks_t callbacks;
    lmqtt_message_callbacks_t message_callbacks;

    struct {
        int (*connect)(struct _lmqtt_client_t *, lmqtt_connect_t *);
        int (*subscribe)(struct _lmqtt_client_t *, lmqtt_subscribe_t *);
        int (*unsubscribe)(struct _lmqtt_client_t *, lmqtt_subscribe_t *);
        int (*publish)(struct _lmqtt_client_t *, lmqtt_publish_t *);
        int (*pingreq)(struct _lmqtt_client_t *);
        int (*disconnect)(struct _lmqtt_client_t *);
    } internal;
} lmqtt_client_t;

void lmqtt_client_initialize(lmqtt_client_t *client, lmqtt_client_callbacks_t
    *callbacks, lmqtt_client_buffers_t *buffers);
void lmqtt_client_reset(lmqtt_client_t *client);
void lmqtt_client_finalize(lmqtt_client_t *client);

int lmqtt_client_connect(lmqtt_client_t *client, lmqtt_connect_t *connect);
int lmqtt_client_subscribe(lmqtt_client_t *client,
    lmqtt_subscribe_t *subscribe);
int lmqtt_client_unsubscribe(lmqtt_client_t *client,
    lmqtt_subscribe_t *subscribe);
int lmqtt_client_publish(lmqtt_client_t *client, lmqtt_publish_t *publish);
int lmqtt_client_disconnect(lmqtt_client_t *client);

void lmqtt_client_set_on_connect(lmqtt_client_t *client,
    lmqtt_client_on_connect_t on_connect, void *on_connect_data);
void lmqtt_client_set_on_subscribe(lmqtt_client_t *client,
    lmqtt_client_on_subscribe_t on_subscribe, void *on_subscribe_data);
void lmqtt_client_set_on_unsubscribe(lmqtt_client_t *client,
    lmqtt_client_on_unsubscribe_t on_unsubscribe, void *on_unsubscribe_data);
void lmqtt_client_set_on_publish(lmqtt_client_t *client,
    lmqtt_client_on_publish_t on_publish, void *on_publish_data);
void lmqtt_client_set_message_callbacks(lmqtt_client_t *client,
    lmqtt_message_callbacks_t *message_callbacks);

void lmqtt_client_set_default_timeout(lmqtt_client_t *client,
    unsigned short secs);
int lmqtt_client_get_timeout(lmqtt_client_t *client, long *secs, long *nsecs);
int lmqtt_client_run_once(lmqtt_client_t *client, lmqtt_string_t **str_rd,
    lmqtt_string_t **str_wr);

#ifdef  __cplusplus
}
#endif

#endif
