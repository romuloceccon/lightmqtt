#include "check_lightmqtt.h"

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
