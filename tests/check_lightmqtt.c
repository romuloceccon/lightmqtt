#include "check_lightmqtt.h"
#include "lightmqtt/io.h"

lmqtt_io_result_t test_buffer_move(test_buffer_t *test_buffer, u8 *dst, u8 *src,
    int len, int *bytes_written)
{
    int cnt = test_buffer->available_len - test_buffer->pos;
    if (cnt > len)
        cnt = len;
    memcpy(dst, src, cnt);
    *bytes_written = cnt;
    test_buffer->pos += cnt;
    test_buffer->call_count += 1;
    return cnt == 0 && test_buffer->available_len < test_buffer->len ?
        LMQTT_IO_AGAIN : LMQTT_IO_SUCCESS;
}

lmqtt_io_result_t test_buffer_read(void *data, u8 *buf, int buf_len,
    int *bytes_read)
{
    test_buffer_t *source = (test_buffer_t *) data;

    return test_buffer_move(source, buf, &source->buf[source->pos], buf_len,
        bytes_read);
}

lmqtt_io_result_t test_buffer_write(void *data, u8 *buf, int buf_len,
    int *bytes_written)
{
    test_buffer_t *destination = (test_buffer_t *) data;

    return test_buffer_move(destination, &destination->buf[destination->pos],
        buf, buf_len, bytes_written);
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
