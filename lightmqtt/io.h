#ifndef _LIGHTMQTT_IO_H_
#define _LIGHTMQTT_IO_H_

#include <lightmqtt/packet.h>

#define LMQTT_RX_BUFFER_SIZE 512
#define LMQTT_TX_BUFFER_SIZE 512

typedef lmqtt_io_result_t (*lmqtt_get_time_t)(long *, long *);

typedef lmqtt_io_result_t (*lmqtt_read_t)(void *, u8 *, int, int *);

typedef lmqtt_io_result_t (*lmqtt_write_t)(void *, u8 *, int, int *);

typedef void (*lmqtt_client_on_connect_t)(void *);

typedef struct _lmqtt_time_t {
    long secs;
    long nsecs;
} lmqtt_time_t;

struct _lmqtt_client_t;

typedef struct _lmqtt_client_t {
    void *data;
    lmqtt_get_time_t get_time;
    lmqtt_read_t read;
    lmqtt_write_t write;

    lmqtt_client_on_connect_t on_connect;
    void *on_connect_data;

    int failed;

    lmqtt_rx_buffer_t rx_state;
    lmqtt_tx_buffer_t tx_state;
    u8 read_buf[LMQTT_RX_BUFFER_SIZE];
    int read_buf_pos;
    u8 write_buf[LMQTT_TX_BUFFER_SIZE];
    int write_buf_pos;
    /* TODO: there should be one of this for each packet in the send queue (when
     * the send queue is implemented) */
    lmqtt_time_t last_req;

    struct {
        int (*connect)(struct _lmqtt_client_t *, lmqtt_connect_t *);
        int (*pingreq)(struct _lmqtt_client_t *);
        int (*disconnect)(struct _lmqtt_client_t *);

        lmqtt_callbacks_t rx_callbacks;

        long keep_alive;
        long timeout;
        int resp_pending;
        lmqtt_time_t last_resp;
        int disconnecting;
    } internal;
} lmqtt_client_t;

typedef enum {
    LMQTT_IO_STATUS_READY = 0,
    LMQTT_IO_STATUS_BLOCK_CONN,
    LMQTT_IO_STATUS_BLOCK_DATA,
    LMQTT_IO_STATUS_ERROR
} lmqtt_io_status_t;

lmqtt_io_status_t process_input(lmqtt_client_t *client);
lmqtt_io_status_t process_output(lmqtt_client_t *client);
lmqtt_io_status_t client_keep_alive(lmqtt_client_t *client);

void lmqtt_client_initialize(lmqtt_client_t *client);
int lmqtt_client_connect(lmqtt_client_t *client, lmqtt_connect_t *connect);
int lmqtt_client_disconnect(lmqtt_client_t *client);
void lmqtt_client_set_on_connect(lmqtt_client_t *client,
    lmqtt_client_on_connect_t on_connect, void *on_connect_data);
void lmqtt_client_set_default_timeout(lmqtt_client_t *client, long secs);
int lmqtt_client_get_timeout(lmqtt_client_t *client, long *secs, long *nsecs);

#endif
