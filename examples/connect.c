#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/select.h>
#include <string.h>

#include "lightmqtt/packet.h"
#include "lightmqtt/io.h"

lmqtt_io_result_t write_data(void *data, u8 *buf, int buf_len,
    int *bytes_written)
{
    int socket_fd = *((int *) data);
    int res;

    res = write(socket_fd, buf, buf_len);
    if (res >= 0) {
        *bytes_written = res;
        fprintf(stderr, "write ok\n");
        return LMQTT_IO_SUCCESS;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        *bytes_written = 0;
        fprintf(stderr, "write again\n");
        return LMQTT_IO_AGAIN;
    }

    fprintf(stderr, "write error: %s\n", strerror(errno));
    return LMQTT_IO_ERROR;
}

int main()
{
    int socket_fd;
    struct sockaddr_in sin;

    lmqtt_client_t client;
    lmqtt_connect_t connect_data;
    lmqtt_io_status_t st = LMQTT_IO_STATUS_BLOCK_CONN;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(socket_fd, F_SETFL, O_NONBLOCK);

    sin.sin_family = AF_INET;
    sin.sin_port = htons(4000);
    if (inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr) == 0) {
        fprintf(stderr, "inet_pton failed\n");
        exit(1);
    }

    if (connect(socket_fd, (struct sockaddr *) &sin, sizeof(sin)) != 0 &&
            errno != EINPROGRESS) {
        fprintf(stderr, "connect failed: %d\n", errno);
        exit(1);
    }

    memset(&client, 0, sizeof(client));
    memset(&connect_data, 0, sizeof(connect_data));

    client.data = &socket_fd;
    client.write = write_data;

    connect_data.keep_alive = 0x102;
    connect_data.client_id.buf = "Rômulo";
    connect_data.client_id.len = 6;

    lmqtt_tx_buffer_connect(&client.tx_state, &connect_data);

    while (st != LMQTT_IO_STATUS_READY) {
        int max_fd = socket_fd + 1;
        fd_set write_set;

        if (st == LMQTT_IO_STATUS_BLOCK_DATA) {
            fprintf(stderr, "client: block data\n");
            exit(1);
        }

        if (st == LMQTT_IO_STATUS_ERROR) {
            fprintf(stderr, "client: error\n");
            exit(1);
        }

        FD_ZERO(&write_set);
        FD_SET(socket_fd, &write_set);

        if (select(max_fd, NULL, &write_set, NULL, NULL) == -1) {
            fprintf(stderr, "select failed: %d\n", errno);
            exit(1);
        }

        fprintf(stderr, "selected\n");
        st = process_output(&client);
    }

    fprintf(stderr, "ok\n");
    return 0;
}
