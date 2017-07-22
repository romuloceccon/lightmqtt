#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "lightmqtt/packet.h"
#include "lightmqtt/client.h"

#include "helpers.h"

static int socket_fd = -1;
static lmqtt_client_t client;
static unsigned short keep_alive;
static unsigned short default_timeout;
static char id[256];

void on_connect(void *data, lmqtt_connect_t *connect, int succeeded)
{
    fprintf(stderr, "connected\n");
}

void fail_connection(unsigned int secs)
{
    socket_close(socket_fd);
    socket_fd = -1;
    fprintf(stderr, "waiting %u seconds...\n", secs);
    sleep(secs);
}

void run(const char *address, unsigned short port)
{
    struct timeval timeout;
    struct timeval *timeout_ptr;

    lmqtt_store_entry_t entries[16];
    unsigned char rx_buffer[128];
    unsigned char tx_buffer[128];
    lmqtt_packet_id_t id_set_items[32];

    lmqtt_connect_t connect_data;
    lmqtt_client_callbacks_t client_callbacks;
    lmqtt_client_buffers_t buffers;

    memset(&connect_data, 0, sizeof(connect_data));
    memset(&client_callbacks, 0, sizeof(client_callbacks));
    memset(&buffers, 0, sizeof(buffers));

    client_callbacks.data = &socket_fd;
    client_callbacks.read = &socket_read;
    client_callbacks.write = &socket_write;
    client_callbacks.get_time = &get_time;

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
    lmqtt_client_set_default_timeout(&client, default_timeout);

    connect_data.keep_alive = keep_alive;
    connect_data.clean_session = 0;
    connect_data.client_id.buf = id;
    connect_data.client_id.len = strlen(id);

    while (1) {
        long secs, nsecs;
        int max_fd;
        fd_set read_set;
        fd_set write_set;
        lmqtt_string_t *str_rd, *str_wr;
        int res;

        if (socket_fd == -1) {
            socket_fd = socket_open(address, port);
            if (socket_fd == -1) {
                fprintf(stderr, "socket_open failed\n");
                exit(1);
            }

            fprintf(stderr, "socket opened\n");
            lmqtt_client_reset(&client);
            lmqtt_client_connect(&client, &connect_data);
        }

        res = lmqtt_client_run_once(&client, &str_rd, &str_wr);

        if (LMQTT_IS_ERROR(res)) {
            fprintf(stderr, "client error: %d\n", LMQTT_ERROR_NUM(res));
            fail_connection(5);
            continue;
        }

        if (LMQTT_IS_EOF_RD(res)) {
            fprintf(stderr, "they disconnected\n");
            fail_connection(5);
            continue;
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

        max_fd = socket_fd + 1;

        fprintf(stderr, "  (selecting)\n");
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
    keep_alive = 5;
    default_timeout = 5;

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
        if (HAS_OPT_ARG("-k")) {
            keep_alive = atoi(argv[i + 1]);
            i += 2;
            continue;
        }
        if (HAS_OPT_ARG("-t")) {
            default_timeout = atoi(argv[i + 1]);
            i += 2;
            continue;
        }
        opt_error = 1;
        break;
    }

    if (opt_error || !address || !strlen(id)) {
        fprintf(stderr, "Syntax error.\n\n");
        fprintf(stderr, "Usage: %s -i <ID> -h <HOST> [-p <PORT>] [-k <SECS>] "
            "[-t <SECS>]\n", argv[0]);
        fprintf(stderr, "    -h HOST    Broker's IP address\n");
        fprintf(stderr, "    -p PORT    Broker's port (default: 1883)\n");
        fprintf(stderr, "    -i ID      Client's id\n");
        fprintf(stderr, "    -k SECS    Keep alive in secs (default: 5)\n");
        fprintf(stderr, "    -t SECS    Timeout in secs (default: 5)\n");
        return 1;
    }

    run(address, port);
    return 0;
}
