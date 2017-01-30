#include <string.h>
#include <assert.h>
#include <lightmqtt/base.h>

typedef struct _LMqttString {
    int len;
    char* buf;
} LMqttString;

typedef struct _LMqttConnect {
    LMqttString client_id;
} LMqttConnect;

#define LMQTT_ENCODE_FINISHED 0
#define LMQTT_ENCODE_AGAIN 1
#define LMQTT_ENCODE_ERROR 2

#define LMQTT_TYPE_CONNECT 0x10

#define LMQTT_STRING_LEN_SIZE 2

#define LMQTT_CONNECT_HEADER_SIZE 10

static int encode_remaining_length(int len, u8 *buf, int buf_len,
    int *bytes_written)
{
    int pos;

    if (len < 0 || len > 0x0fffffff || buf_len < 1 || buf_len < 2 && len > 0x7f
            || buf_len < 3 && len > 0x3fff || buf_len < 4 && len > 0x1fffff)
        return LMQTT_ENCODE_ERROR;

    pos = 0;
    do {
        u8 b = len % 128;
        len /= 128;
        buf[pos++] = len > 0 ? b | 0x80 : b;
    } while (len > 0);

    *bytes_written = pos;
    return LMQTT_ENCODE_FINISHED;
}

static int calc_connect_remaining_legth(LMqttConnect *connect)
{
    return LMQTT_CONNECT_HEADER_SIZE + connect->client_id.len +
        LMQTT_STRING_LEN_SIZE;
}

static int encode_connect_fixed_header(LMqttConnect *connect, int offset,
    u8 *buf, int buf_len, int *bytes_written)
{
    int remain_len_size;

    assert(offset == 0);

    if (encode_remaining_length(calc_connect_remaining_legth(connect), buf + 1,
            buf_len - 1, &remain_len_size) != LMQTT_ENCODE_FINISHED)
        return LMQTT_ENCODE_ERROR;

    buf[0] = LMQTT_TYPE_CONNECT;

    *bytes_written = 1 + remain_len_size;
    return LMQTT_ENCODE_FINISHED;
}

static int encode_connect_variable_header(LMqttConnect *connect, int offset,
    u8 *buf, int buf_len, int *bytes_written)
{
    assert(offset == 0);

    if (buf_len < LMQTT_CONNECT_HEADER_SIZE)
        return LMQTT_ENCODE_ERROR;

    buf[0] = 0x00;
    buf[1] = 0x04;
    buf[2] = 'M';
    buf[3] = 'Q';
    buf[4] = 'T';
    buf[5] = 'T';

    buf[6] = 0x04;
    buf[7] = 0x00;
    buf[8] = 0x00;
    buf[9] = 0x00;

    *bytes_written = LMQTT_CONNECT_HEADER_SIZE;
    return LMQTT_ENCODE_FINISHED;
}
