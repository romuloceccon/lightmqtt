#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/select.h>
#include <string.h>
#include <unistd.h>

#include "lightmqtt/packet.h"
#include "lightmqtt/io.h"

lmqtt_io_result_t read_data(void *data, u8 *buf, int buf_len,
    int *bytes_read)
{
    int socket_fd = *((int *) data);
    int res;

    res = read(socket_fd, buf, buf_len);
    if (res >= 0) {
        int i;
        *bytes_read = res;
        fprintf(stderr, "read ok: ");
        if (res > 0)
            for (i = 0; i < res; i++)
                fprintf(stderr, "%02x ", (unsigned int) buf[i]);
        else
            fprintf(stderr, "eof");
        fprintf(stderr, "\n");
        return LMQTT_IO_SUCCESS;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        *bytes_read = 0;
        fprintf(stderr, "read again\n");
        return LMQTT_IO_AGAIN;
    }

    fprintf(stderr, "read error: %s\n", strerror(errno));
    return LMQTT_IO_ERROR;
}

lmqtt_io_result_t write_data(void *data, u8 *buf, int buf_len,
    int *bytes_written)
{
    int socket_fd = *((int *) data);
    int res;

    res = write(socket_fd, buf, buf_len);
    if (res >= 0) {
        int i;
        *bytes_written = res;
        fprintf(stderr, "write ok: ");
        for (i = 0; i < res; i++)
            fprintf(stderr, "%02x ", (unsigned int) buf[i]);
        fprintf(stderr, "\n");
        return LMQTT_IO_SUCCESS;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EPIPE) {
        *bytes_written = 0;
        fprintf(stderr, "write again\n");
        return LMQTT_IO_AGAIN;
    }

    fprintf(stderr, "write error: %s\n", strerror(errno));
    return LMQTT_IO_ERROR;
}

lmqtt_io_result_t get_time(long *secs, long *nsecs)
{
    struct timespec tim;

    if (clock_gettime(CLOCK_MONOTONIC, &tim) == 0) {
        *secs = tim.tv_sec;
        *nsecs = tim.tv_nsec;
        return LMQTT_IO_SUCCESS;
    }

    fprintf(stderr, "clock_gettime error: %s\n", strerror(errno));
    return LMQTT_IO_ERROR;
}

static char *topic = "topic";
static lmqtt_subscribe_t subscribe;
static lmqtt_subscription_t subscriptions[1];
static lmqtt_publish_t publish;
static char payload[16];

void on_connect(void *data, lmqtt_connect_t *connect, int succeeded)
{
    lmqtt_client_t *client;

    fprintf(stderr, "connected!\n");

    client = (lmqtt_client_t *) data;

    memset(&subscribe, 0, sizeof(subscribe));
    memset(subscriptions, 0, sizeof(subscriptions));
    subscribe.count = 1;
    subscribe.subscriptions = subscriptions;
    subscriptions[0].qos = 1;
    subscriptions[0].topic.buf = topic;
    subscriptions[0].topic.len = strlen(topic);

    lmqtt_client_subscribe(client, &subscribe);
}

void on_subscribe(void *data, lmqtt_subscribe_t *subscribe, int succeeded)
{
    lmqtt_client_t *client;

    fprintf(stderr, "subscribed: %d!\n", succeeded);

    client = (lmqtt_client_t *) data;

    memset(&publish, 0, sizeof(publish));
    publish.qos = 2;
    publish.topic.buf = "a/b/c";
    publish.topic.len = strlen(publish.topic.buf);
    publish.payload.buf = payload;
    publish.payload.len = sizeof(payload);

    memset(payload, 'x', sizeof(payload));

    lmqtt_client_publish(client, &publish);
}

void on_publish(void *data, lmqtt_publish_t *publish, int succeeded)
{
    fprintf(stderr, "published: %d!\n", succeeded);
}

int main()
{
    int socket_fd;
    struct sockaddr_in sin;
    struct timeval timeout;
    struct timeval *timeout_ptr;
    int cnt = 0;
    lmqtt_store_entry_t entries[16];
    u8 rx_buffer[256];
    u8 tx_buffer[256];

    lmqtt_client_t client;
    lmqtt_connect_t connect_data;
    lmqtt_client_callbacks_t callbacks;
    lmqtt_client_buffers_t buffers;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(socket_fd, F_SETFL, O_NONBLOCK);

    sin.sin_family = AF_INET;
    sin.sin_port = htons(1883);
    if (inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr) == 0) {
        fprintf(stderr, "inet_pton failed\n");
        exit(1);
    }

    if (connect(socket_fd, (struct sockaddr *) &sin, sizeof(sin)) != 0 &&
            errno != EINPROGRESS) {
        fprintf(stderr, "connect failed: %d\n", errno);
        exit(1);
    }

    callbacks.data = &socket_fd;
    callbacks.read = read_data;
    callbacks.write = write_data;
    callbacks.get_time = get_time;

    buffers.store_size = sizeof(entries);
    buffers.store = entries;
    buffers.rx_buffer_size = sizeof(rx_buffer);
    buffers.rx_buffer = rx_buffer;
    buffers.tx_buffer_size = sizeof(tx_buffer);
    buffers.tx_buffer = tx_buffer;

    lmqtt_client_initialize(&client, &callbacks, &buffers);

    lmqtt_client_set_on_connect(&client, on_connect, &client);
    lmqtt_client_set_on_subscribe(&client, on_subscribe, &client);
    lmqtt_client_set_on_publish(&client, on_publish, &client);
    lmqtt_client_set_default_timeout(&client, 5);

    memset(&connect_data, 0, sizeof(connect_data));
    connect_data.keep_alive = 3;
    connect_data.clean_session = 1;
    connect_data.client_id.buf = "Rômulo";
    connect_data.client_id.len = 7;

    lmqtt_client_connect(&client, &connect_data);

    while (1) {
        long secs, nsecs;
        int max_fd = socket_fd + 1;
        fd_set read_set;
        fd_set write_set;
        lmqtt_string_t *str_rd, *str_wr;
        int res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

        if (LMQTT_IS_ERROR(res)) {
            fprintf(stderr, "client: error\n");
            close(socket_fd);
            exit(1);
        }

        if (LMQTT_IS_EOF_RD(res)) {
            fprintf(stderr, "they disconnected!\n");
            close(socket_fd);
            exit(0);
        }

        if (LMQTT_IS_EOF_WR(res)) {
            fprintf(stderr, "we disconnected!\n");
            close(socket_fd);
            exit(0);
        }

        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        if (LMQTT_WOULD_BLOCK_CONN_RD(res))
            FD_SET(socket_fd, &read_set);
        if (LMQTT_WOULD_BLOCK_CONN_WR(res))
            FD_SET(socket_fd, &write_set);
        if (lmqtt_client_get_timeout(&client, &secs, &nsecs)) {
            timeout.tv_sec = secs;
            timeout.tv_usec = nsecs / 1000;
            timeout_ptr = &timeout;
        } else {
            timeout_ptr = NULL;
        }

        cnt += 1;
        if (secs >= 2 && cnt >= 10) {
            lmqtt_client_disconnect(&client);
            continue;
        }

        if (timeout_ptr)
            fprintf(stderr, "selecting (%ld, %ld) ... ", secs, nsecs);
        else
            fprintf(stderr, "selecting ... ");

        switch (select(max_fd, &read_set, &write_set, NULL, timeout_ptr)) {
            case -1:
                fprintf(stderr, "failed: %d!\n", errno);
                exit(1);
            case 0:
                fprintf(stderr, "timed out\n");
                break;
            default:
                fprintf(stderr, "signaled\n");
        }
    }
}
