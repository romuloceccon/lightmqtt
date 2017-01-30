#include <string.h>
#include <assert.h>
#include <lightmqtt/base.h>

typedef struct _LMqttString {
    int len;
    char* buf;
} LMqttString;

typedef struct _LMqttConnect {
    u16 keep_alive;
    int clean_session;
    int qos;
    int will_retain;
    LMqttString client_id;
    LMqttString will_topic;
    LMqttString will_message;
    LMqttString user_name;
    LMqttString password;
} LMqttConnect;

#define LMQTT_ENCODE_FINISHED 0
#define LMQTT_ENCODE_AGAIN 1
#define LMQTT_ENCODE_ERROR 2

#define LMQTT_TYPE_CONNECT 0x10

#define LMQTT_FLAG_CLEAN_SESSION 0x02
#define LMQTT_FLAG_WILL_FLAG 0x04
#define LMQTT_FLAG_WILL_RETAIN 0x20
#define LMQTT_FLAG_PASSWORD_FLAG 0x40
#define LMQTT_FLAG_USER_NAME_FLAG 0x80
#define LMQTT_OFFSET_FLAG_QOS 3

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

static int calc_connect_payload_field_length(LMqttString *str)
{
    return str->len > 0 ? LMQTT_STRING_LEN_SIZE + str->len : 0;
}

static int calc_connect_remaining_legth(LMqttConnect *connect)
{
    return LMQTT_CONNECT_HEADER_SIZE +
        calc_connect_payload_field_length(&connect->client_id) +
        calc_connect_payload_field_length(&connect->will_topic) +
        calc_connect_payload_field_length(&connect->will_message) +
        calc_connect_payload_field_length(&connect->user_name) +
        calc_connect_payload_field_length(&connect->password);
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
    u16 ka;
    u8 flags;

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

    flags = connect->qos << LMQTT_OFFSET_FLAG_QOS;
    if (connect->clean_session)
        flags |= LMQTT_FLAG_CLEAN_SESSION;
    if (connect->will_retain)
        flags |= LMQTT_FLAG_WILL_RETAIN;
    if (connect->will_topic.len > 0)
        flags |= LMQTT_FLAG_WILL_FLAG;
    if (connect->user_name.len > 0)
        flags |= LMQTT_FLAG_USER_NAME_FLAG;
    if (connect->password.len > 0)
        flags |= LMQTT_FLAG_PASSWORD_FLAG;
    buf[7] = flags;

    ka = connect->keep_alive;
    buf[8] = (ka >> 8) & 0xff;
    buf[9] = (ka >> 0) & 0xff;

    *bytes_written = LMQTT_CONNECT_HEADER_SIZE;
    return LMQTT_ENCODE_FINISHED;
}
