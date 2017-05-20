#ifndef _EXAMPLES_HELPERS_H
#define _EXAMPLES_HELPERS_H

#include "lightmqtt/core.h"

lmqtt_io_result_t get_time(long *secs, long *nsecs);
lmqtt_io_result_t socket_read(void *data, u8 *buf, int buf_len,
    int *bytes_read);
lmqtt_io_result_t socket_write(void *data, u8 *buf, int buf_len,
    int *bytes_written);
int socket_open(const char *address, unsigned short port);
void socket_close(int fd);

#endif
