#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#include "lightmqtt/packet.h"
#include "lightmqtt/client.h"

#include "helpers.h"

static lmqtt_client_t client;

static char id[256];
static char to[256];
static char msg[256];

static int count = 0;
static lmqtt_subscribe_t subscribe;
static lmqtt_subscription_t subscriptions[1];
static lmqtt_publish_t publish;
static char payload[256];
static char message_topic[256];
static char message_payload[256];

int on_connect(void *data, lmqtt_connect_t *connect, int succeeded)
{
    if (!succeeded)
        return 1;

    memset(&subscribe, 0, sizeof(subscribe));
    memset(subscriptions, 0, sizeof(subscriptions));
    subscribe.count = 1;
    subscribe.subscriptions = subscriptions;
    subscriptions[0].requested_qos = LMQTT_QOS_2;
    subscriptions[0].topic.buf = id;
    subscriptions[0].topic.len = strlen(id);

    lmqtt_client_subscribe(&client, &subscribe);
    return 1;
}

int on_subscribe(void *data, lmqtt_subscribe_t *subscribe, int succeeded)
{
    if (!succeeded || strlen(msg) == 0)
        return 1;

    memset(&publish, 0, sizeof(publish));
    publish.qos = LMQTT_QOS_2;
    publish.topic.buf = to;
    publish.topic.len = strlen(to);
    publish.payload.buf = payload;
    publish.payload.len = strlen(msg);
    memcpy(payload, msg, publish.payload.len);

    lmqtt_client_publish(&client, &publish);
    return 1;
}

int on_message(void *data, lmqtt_publish_t *message)
{
    fprintf(stderr, "%.*s (%d): %.*s\n", (int) message->topic.len,
        message->topic.buf, count++, (int) message->payload.len,
        message->payload.buf);

    memset(&publish, 0, sizeof(publish));
    publish.qos = LMQTT_QOS_2;
    publish.topic.buf = to;
    publish.topic.len = strlen(to);
    publish.payload.buf = payload;
    publish.payload.len = message->payload.len;
    memcpy(payload, message->payload.buf, publish.payload.len);

    lmqtt_client_publish(&client, &publish);
    return 1;
}

lmqtt_allocate_result_t on_message_allocate_topic(void *data,
    lmqtt_publish_t *publish, size_t size)
{
    publish->topic.buf = message_topic;
    publish->topic.len = size;
    return LMQTT_ALLOCATE_SUCCESS;
}

lmqtt_allocate_result_t on_message_allocate_payload(void *data,
    lmqtt_publish_t *publish, size_t size)
{
    publish->payload.buf = message_payload;
    publish->payload.len = size;
    return LMQTT_ALLOCATE_SUCCESS;
}

void run(const char *address, unsigned short port)
{
    int socket_fd;
    struct timeval timeout;
    struct timeval *timeout_ptr;

    lmqtt_store_entry_t entries[16];
    unsigned char rx_buffer[128];
    unsigned char tx_buffer[128];
    lmqtt_packet_id_t id_set_items[32];

    lmqtt_connect_t connect_data;
    lmqtt_client_callbacks_t client_callbacks;
    lmqtt_message_callbacks_t message_callbacks;
    lmqtt_client_buffers_t buffers;

    socket_fd = socket_open(address, port);
    if (socket_fd == -1) {
        fprintf(stderr, "socket_open failed\n");
        exit(1);
    }

    memset(&connect_data, 0, sizeof(connect_data));
    memset(&client_callbacks, 0, sizeof(client_callbacks));
    memset(&message_callbacks, 0, sizeof(message_callbacks));
    memset(&buffers, 0, sizeof(buffers));

    client_callbacks.data = &socket_fd;
    client_callbacks.read = &socket_read;
    client_callbacks.write = &socket_write;
    client_callbacks.get_time = &get_time;

    message_callbacks.on_publish = &on_message;
    message_callbacks.on_publish_allocate_topic = &on_message_allocate_topic;
    message_callbacks.on_publish_allocate_payload = &on_message_allocate_payload;
    message_callbacks.on_publish_data = &client;

    buffers.store_size = sizeof(entries);
    buffers.store = entries;
    buffers.rx_buffer_size = sizeof(rx_buffer);
    buffers.rx_buffer = rx_buffer;
    buffers.tx_buffer_size = sizeof(tx_buffer);
    buffers.tx_buffer = tx_buffer;
    buffers.id_set_size = sizeof(id_set_items);
    buffers.id_set = id_set_items;

    lmqtt_client_initialize(&client, &client_callbacks, &buffers);

    lmqtt_client_set_on_connect(&client, on_connect, &client);
    lmqtt_client_set_on_subscribe(&client, on_subscribe, &client);
    lmqtt_client_set_message_callbacks(&client, &message_callbacks);
    lmqtt_client_set_default_timeout(&client, 5);

    connect_data.keep_alive = 3;
    connect_data.clean_session = 1;
    connect_data.client_id.buf = id;
    connect_data.client_id.len = strlen(id);

    lmqtt_client_connect(&client, &connect_data);

    while (1) {
        long secs, nsecs;
        int max_fd = socket_fd + 1;
        fd_set read_set;
        fd_set write_set;
        lmqtt_string_t *str_rd, *str_wr;
        int res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

        if (LMQTT_IS_ERROR(res)) {
            fprintf(stderr, "client error: %d\n", LMQTT_ERROR_NUM(res));
            exit(1);
        }

        if (LMQTT_IS_EOF_RD(res)) {
            fprintf(stderr, "they disconnected\n");
            exit(0);
        }

        if (LMQTT_IS_EOF_WR(res)) {
            fprintf(stderr, "we disconnected\n");
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

        if (select(max_fd, &read_set, &write_set, NULL, timeout_ptr) == -1) {
            fprintf(stderr, "select failed: %d!\n", errno);
            exit(1);
        }
    }
}

#define HAS_OPT_ARG(str) (i + 1 < argc && strcmp(str, argv[i]) == 0)

int main(int argc, const char *argv[])
{
    const char *address = NULL;
    unsigned short port = 1883;
    int opt_error = 0;

    strcpy(id, "");
    strcpy(to, "");
    strcpy(msg, "");

    for (int i = 1; i < argc; ) {
        if (HAS_OPT_ARG("-h")) {
            address = argv[i + 1];
            i += 2;
            continue;
        }
        if (HAS_OPT_ARG("-p")) {
            port = atoi(argv[i + 1]);
            i += 2;
            continue;
        }
        if (HAS_OPT_ARG("-i")) {
            strcpy(id, argv[i + 1]);
            i += 2;
            continue;
        }
        if (HAS_OPT_ARG("-t")) {
            strcpy(to, argv[i + 1]);
            i += 2;
            continue;
        }
        if (HAS_OPT_ARG("-m")) {
            strcpy(msg, argv[i + 1]);
            i += 2;
            continue;
        }
        opt_error = 1;
        break;
    }

    if (opt_error || !address || !strlen(id) || !strlen(to)) {
        fprintf(stderr, "Syntax error.\n\n");
        fprintf(stderr, "Usage: %s -i <ID> -t <TO> -h <HOST> [-p <PORT>] "
            "[-m <MSG>]\n", argv[0]);
        fprintf(stderr, "    -h HOST    Broker's IP address\n");
        fprintf(stderr, "    -p PORT    Broker's port (default: 1883)\n");
        fprintf(stderr, "    -i ID      This client's id\n");
        fprintf(stderr, "    -t TO      Other client's id\n");
        fprintf(stderr, "    -m MSG     Message to send to other client "
            "(default: <empty>)\n");
        return 1;
    }

    run(address, port);
    return 0;
}
