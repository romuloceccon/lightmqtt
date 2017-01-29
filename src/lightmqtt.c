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

static int encode_connect_fixed_header(LMqttConnect* connect, int offset,
    u8* buf, int buf_len, int* bytes_written)
{
    int i;
    int remaining_len;
    int fixed_len;

    buf[0] = LMQTT_TYPE_CONNECT;

    i = 1;
    remaining_len = LMQTT_CONNECT_HEADER_SIZE + connect->client_id.len +
        LMQTT_STRING_LEN_SIZE;
    do {
        u8 s = remaining_len % 128;
        remaining_len /= 128;
        buf[i++] = remaining_len > 0 ? s | 0x80 : s;
    } while (remaining_len > 0);
    fixed_len = i;

    buf[i + 0] = 0x00;
    buf[i + 1] = 0x04;
    buf[i + 2] = 'M';
    buf[i + 3] = 'Q';
    buf[i + 4] = 'T';
    buf[i + 5] = 'T';

    buf[i + 6] = 0x04;
    buf[i + 7] = 0x00;
    buf[i + 8] = 0x00;
    buf[i + 9] = 0x00;

    buf[i + 10] = 0x00;
    buf[i + 11] = connect->client_id.len;
    strncpy(&buf[i + 12], connect->client_id.buf, connect->client_id.len);

    *bytes_written = fixed_len + LMQTT_CONNECT_HEADER_SIZE +
        connect->client_id.len + LMQTT_STRING_LEN_SIZE;

    return LMQTT_ENCODE_FINISHED;
}

static int encode_connect_variable_header(LMqttConnect* connect, int offset,
    u8* buf, int buf_len, int* bytes_written)
{
    return LMQTT_ENCODE_ERROR;
}
