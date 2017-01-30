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

#define STRING_LEN_BYTE(val, num) (((val) >> ((num) * 8)) & 0xff)

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
        /* client_id is always present in payload */
        LMQTT_STRING_LEN_SIZE + connect->client_id.len +
        calc_connect_payload_field_length(&connect->will_topic) +
        calc_connect_payload_field_length(&connect->will_message) +
        calc_connect_payload_field_length(&connect->user_name) +
        calc_connect_payload_field_length(&connect->password);
}

static int validate_payload_field_length(LMqttString *str)
{
    return str->len >= 0 && str->len <= 0xffff;
}

static int validate_connect(LMqttConnect *connect)
{
    if (!validate_payload_field_length(&connect->client_id) ||
            !validate_payload_field_length(&connect->will_topic) ||
            !validate_payload_field_length(&connect->will_message) ||
            !validate_payload_field_length(&connect->user_name) ||
            !validate_payload_field_length(&connect->password))
        return 0;

    if (connect->will_topic.len == 0 ^ connect->will_message.len == 0)
        return 0;

    if (connect->will_topic.len == 0 && connect->will_retain)
        return 0;

    if (connect->client_id.len == 0 && !connect->clean_session)
        return 0;

    if (connect->user_name.len == 0 && connect->password.len != 0)
        return 0;

    if (connect->qos < 0 || connect->qos > 2)
        return 0;

    return 1;
}

static int encode_connect_fixed_header(LMqttConnect *connect, int offset,
    u8 *buf, int buf_len, int *bytes_written)
{
    int remain_len_size;

    assert(offset == 0 && buf_len > 0);

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
    u8 flags;

    assert(offset == 0 && buf_len > 0);

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

    buf[8] = STRING_LEN_BYTE(connect->keep_alive, 1);
    buf[9] = STRING_LEN_BYTE(connect->keep_alive, 0);

    *bytes_written = LMQTT_CONNECT_HEADER_SIZE;
    return LMQTT_ENCODE_FINISHED;
}

static int encode_connect_payload_client_id(LMqttConnect *connect, int offset,
    u8 *buf, int buf_len, int *bytes_written)
{
    int len = connect->client_id.len;
    int result;
    int pos = 0;
    int offset_str;
    int i;

    assert(offset < buf_len && buf_len > 0);

    for (i = 0; i < LMQTT_STRING_LEN_SIZE; i++) {
        if (offset <= i)
            buf[pos++] = STRING_LEN_BYTE(len, LMQTT_STRING_LEN_SIZE - i - 1);
    }

    offset_str = offset <= LMQTT_STRING_LEN_SIZE ? 0 :
        offset - LMQTT_STRING_LEN_SIZE;
    len -= offset_str;

    if (len > buf_len - pos) {
        len = buf_len - pos;
        result = LMQTT_ENCODE_AGAIN;
    } else {
        result = LMQTT_ENCODE_FINISHED;
    }

    memcpy(buf + pos, connect->client_id.buf + offset_str, len);
    *bytes_written = pos + len;
    return result;
}
