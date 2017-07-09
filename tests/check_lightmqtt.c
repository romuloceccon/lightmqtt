#include "check_lightmqtt.h"
#include "lightmqtt/client.h"

lmqtt_io_result_t test_buffer_move(test_buffer_t *test_buffer, void *dst,
    void *src, size_t len, size_t *bytes_written)
{
    size_t cnt = test_buffer->available_len - test_buffer->pos;
    if (cnt > len)
        cnt = len;
    memcpy(dst, src, cnt);
    *bytes_written = cnt;
    test_buffer->pos += cnt;
    test_buffer->call_count += 1;
    return cnt == 0 && test_buffer->available_len < test_buffer->len ?
        LMQTT_IO_WOULD_BLOCK : LMQTT_IO_SUCCESS;
}

lmqtt_io_result_t test_buffer_read(void *data, void *buf, size_t buf_len,
    size_t *bytes_read, int *os_error)
{
    test_buffer_t *source = (test_buffer_t *) data;

    return test_buffer_move(source, buf, &source->buf[source->pos], buf_len,
        bytes_read);
}

lmqtt_io_result_t test_buffer_write(void *data, void *buf, size_t buf_len,
    size_t *bytes_written, int *os_error)
{
    test_buffer_t *destination = (test_buffer_t *) data;

    return test_buffer_move(destination, &destination->buf[destination->pos],
        buf, buf_len, bytes_written);
}

lmqtt_io_result_t test_buffer_io_fail(void *data, void *buf, size_t buf_len,
    size_t *byte_cnt, int *os_error)
{
    *byte_cnt = 0;
    *os_error = 1;
    return LMQTT_IO_ERROR;
}

static lmqtt_time_t test_time;

lmqtt_io_result_t test_time_get(long *secs, long *nsecs)
{
    *secs = test_time.secs;
    *nsecs = test_time.nsecs;
    return LMQTT_IO_SUCCESS;
}

void test_time_set(long secs, long nsecs)
{
    test_time.secs = secs;
    test_time.nsecs = nsecs;
}

lmqtt_io_result_t test_socket_read(void *data, void *buf, size_t buf_len,
    size_t *bytes_read, int *os_error)
{
    test_socket_t *sock = (test_socket_t *) data;
    return test_buffer_read(&sock->read_buf, buf, buf_len, bytes_read,
        os_error);
}

lmqtt_io_result_t test_socket_write(void *data, void *buf, size_t buf_len,
    size_t *bytes_written, int *os_error)
{
    test_socket_t *sock = (test_socket_t *) data;
    return test_buffer_write(&sock->write_buf, buf, buf_len, bytes_written,
        os_error);
}

void test_socket_init(test_socket_t *socket)
{
    memset(socket, 0, sizeof(*socket));
    socket->read_buf.len = sizeof(socket->read_buf.buf);
    socket->write_buf.len = sizeof(socket->write_buf.buf);
    socket->write_buf.available_len = socket->write_buf.len;
}

void test_socket_append_param(test_socket_t *socket, int val, int param)
{
    test_buffer_t *buf = &socket->read_buf;

    char src[4096];
    size_t len = 0;
    size_t rem_len;

    switch (val) {
        case TEST_CONNACK_SUCCESS:
            memcpy(src, "\x20\x02\x00\x00", 4);
            len = 4;
            break;
        case TEST_CONNACK_FAILURE:
            memcpy(src, "\x20\x02\x00\x01", 4);
            len = 4;
            break;
        case TEST_SUBACK_SUCCESS:
            memcpy(src, "\x90\x03\x00\x00\x00", 5);
            src[2] = param >> 8;
            src[3] = param;
            len = 5;
            break;
        case TEST_UNSUBACK_SUCCESS:
            memcpy(src, "\xb0\x02\x00\x00", 4);
            src[2] = param >> 8;
            src[3] = param;
            len = 4;
            break;
        case TEST_PUBLISH_QOS_2:
            memcpy(src, "\x34\x06\x00\x01X\x00\x00X", 8);
            src[5] = param >> 8;
            src[6] = param;
            len = 8;
            break;
        case TEST_PUBLISH_QOS_0_BIG:
            rem_len = sizeof(src) - 3;
            memset(src, 'x', sizeof(src));
            memcpy(src, "\x30\x00\x00\x00\x01X\x00\x00", 8);
            src[1] = (rem_len & 0x7f) | 0x80;
            src[2] = rem_len >> 7;
            src[6] = param >> 8;
            src[7] = param;
            len = sizeof(src);
            break;
        case TEST_PUBACK:
            memcpy(src, "\x40\x02\x00\x00", 4);
            src[2] = param >> 8;
            src[3] = param;
            len = 4;
            break;
        case TEST_PUBREC:
            memcpy(src, "\x50\x02\x00\x00", 4);
            src[2] = param >> 8;
            src[3] = param;
            len = 4;
            break;
        case TEST_PUBCOMP:
            memcpy(src, "\x70\x02\x00\x00", 4);
            src[2] = param >> 8;
            src[3] = param;
            len = 4;
            break;
        case TEST_PINGRESP:
            memcpy(src, "\xd0\x00", 2);
            len = 2;
            break;
    }

    if (src) {
        memcpy(&buf->buf[socket->test_pos_read], src, len);
        socket->test_pos_read += len;
        buf->available_len += len;
    }
}

void test_socket_append(test_socket_t *socket, int val)
{
    test_socket_append_param(socket, val, 0);
}

int test_socket_shift(test_socket_t *socket)
{
    test_buffer_t *buf = &socket->write_buf;

    test_type_request_t result = -4; /* invalid command */
    unsigned char cmd;
    unsigned char remain_len;
    size_t len;
    size_t available = buf->pos - socket->test_pos_write;

    if (available <= 0)
        return -1; /* eof */
    if (available < 2)
        return -2; /* partial buffer */

    cmd = buf->buf[socket->test_pos_write];
    remain_len = buf->buf[socket->test_pos_write + 1];

    if (remain_len & 0x80)
        return -3; /* invalid remaining length (for simplicity we do not
                      implement here decoding for values larger than 127) */

    len = 2 + remain_len;
    if (available < len)
        return -2; /* partial buffer */

    switch (cmd & 0xf0) {
        case 0x10:
            result = TEST_CONNECT;
            break;
        case 0x80:
            result = TEST_SUBSCRIBE;
            break;
        case 0xa0:
            result = TEST_UNSUBSCRIBE;
            break;
        case 0x30:
            result = TEST_PUBLISH;
            break;
        case 0x60:
            result = TEST_PUBREL;
            break;
        case 0xc0:
            result = TEST_PINGREQ;
            break;
        case 0xe0:
            result = TEST_DISCONNECT;
            break;
    }

    socket->test_pos_write += len;
    return result;
}
