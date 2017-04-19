#include <lightmqtt/packet.h>
#include <string.h>
#include <assert.h>

#define LMQTT_TYPE_MIN 1
#define LMQTT_TYPE_CONNECT 1
#define LMQTT_TYPE_CONNACK 2
#define LMQTT_TYPE_PUBLISH 3
#define LMQTT_TYPE_PUBACK 4
#define LMQTT_TYPE_PUBREC 5
#define LMQTT_TYPE_PUBREL 6
#define LMQTT_TYPE_PUBCOMP 7
#define LMQTT_TYPE_SUBSCRIBE 8
#define LMQTT_TYPE_SUBACK 9
#define LMQTT_TYPE_UNSUBSCRIBE 10
#define LMQTT_TYPE_UNSUBACK 11
#define LMQTT_TYPE_PINGREQ 12
#define LMQTT_TYPE_PINGRESP 13
#define LMQTT_TYPE_DISCONNECT 14
#define LMQTT_TYPE_MAX 14

#define LMQTT_FLAG_CLEAN_SESSION 0x02
#define LMQTT_FLAG_WILL_FLAG 0x04
#define LMQTT_FLAG_WILL_RETAIN 0x20
#define LMQTT_FLAG_PASSWORD_FLAG 0x40
#define LMQTT_FLAG_USER_NAME_FLAG 0x80
#define LMQTT_OFFSET_FLAG_QOS 3

#define LMQTT_STRING_LEN_SIZE 2
#define LMQTT_PACKET_ID_SIZE 2

#define STRING_LEN_BYTE(val, num) (((val) >> ((num) * 8)) & 0xff)

/******************************************************************************
 * GENERAL PRIVATE functions
 ******************************************************************************/

/* caller must guarantee buf is at least 4-bytes long! */
static lmqtt_encode_result_t encode_remaining_length(int len, u8 *buf,
    int *bytes_written)
{
    int pos;

    if (len < 0 || len > 0x0fffffff)
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

/******************************************************************************
 * lmqtt_encode_buffer_t PRIVATE functions
 ******************************************************************************/

static lmqtt_encode_result_t encode_buffer_encode(
    lmqtt_encode_buffer_t *encode_buffer, lmqtt_store_value_t *value,
    encode_buffer_builder_t builder, int offset, u8 *buf, int buf_len,
    int *bytes_written)
{
    int cnt;
    int result;

    assert(buf_len >= 0);

    if (!encode_buffer->encoded) {
        if (builder(value, encode_buffer) != LMQTT_ENCODE_FINISHED)
            return LMQTT_ENCODE_ERROR;
        encode_buffer->encoded = 1;
    }
    assert(encode_buffer->buf_len > 0 && offset < encode_buffer->buf_len);

    cnt = encode_buffer->buf_len - offset;
    result = LMQTT_ENCODE_FINISHED;

    if (cnt > buf_len) {
        cnt = buf_len;
        result = LMQTT_ENCODE_CONTINUE;
    }

    memcpy(buf, &encode_buffer->buf[offset], cnt);
    *bytes_written = cnt;

    if (result == LMQTT_ENCODE_FINISHED)
        memset(encode_buffer, 0, sizeof(*encode_buffer));

    return result;
}

/******************************************************************************
 * lmqtt_string_t PRIVATE functions
 ******************************************************************************/

static lmqtt_read_result_t string_fetch(lmqtt_string_t *str, int offset,
    u8 *buf, int buf_len, int *bytes_written)
{
    if (str->read != 0)
        /* `offset` is not used with the callback. We're actually trusting the
         * encoding functions and the callback work the same way, i.e. in each
         * call the offset is equal to the previous offset plus the previous
         * read byte count. Maybe `offset` could be eliminated and those
         * expectations documented? */
        return str->read(str->data, buf, buf_len, bytes_written);

    memcpy(buf, str->buf + offset, buf_len);
    *bytes_written = buf_len;
    return LMQTT_READ_SUCCESS;
}

static lmqtt_encode_result_t string_encode(lmqtt_string_t *str, int encode_len,
    int encode_if_empty, int offset, u8 *buf, int buf_len, int *bytes_written,
    lmqtt_string_t **blocking_str)
{
    int len = str->len;
    int result;
    int pos = 0;
    int offset_str = offset;
    int i;

    int read_cnt;
    int read_res;
    int remaining;

    *bytes_written = 0;
    *blocking_str = NULL;

    if (len == 0 && !encode_if_empty)
        return LMQTT_ENCODE_FINISHED;

    assert(buf_len > 0);
    assert(offset < len + (encode_len ? LMQTT_STRING_LEN_SIZE : 0));

    if (encode_len) {
        for (i = 0; i < LMQTT_STRING_LEN_SIZE; i++) {
            if (offset <= i) {
                buf[pos++] = STRING_LEN_BYTE(len, LMQTT_STRING_LEN_SIZE - i - 1);
                *bytes_written += 1;
                if (pos >= buf_len) {
                    return pos >= LMQTT_STRING_LEN_SIZE && len == 0 ?
                        LMQTT_ENCODE_FINISHED : LMQTT_ENCODE_CONTINUE;
                }
            }
        }
        offset_str = offset <= LMQTT_STRING_LEN_SIZE ? 0 :
            offset - LMQTT_STRING_LEN_SIZE;
    }

    len -= offset_str;
    remaining = str->len - offset_str;

    if (len > buf_len - pos)
        len = buf_len - pos;

    read_res = string_fetch(str, offset_str, buf + pos, len, &read_cnt);
    *bytes_written += read_cnt;
    assert(read_cnt <= remaining);

    if (read_res == LMQTT_READ_WOULD_BLOCK && read_cnt == 0) {
        *blocking_str = str;
        result = LMQTT_ENCODE_WOULD_BLOCK;
    } else if (read_res == LMQTT_READ_SUCCESS && read_cnt >= remaining) {
        result = LMQTT_ENCODE_FINISHED;
    } else if (read_res == LMQTT_READ_SUCCESS && read_cnt > 0) {
        result = LMQTT_ENCODE_CONTINUE;
    } else {
        result = LMQTT_ENCODE_ERROR;
    }

    return result;
}

static int string_calc_field_length(lmqtt_string_t *str)
{
    return str->len > 0 ? LMQTT_STRING_LEN_SIZE + str->len : 0;
}

static int string_validate_field_length(lmqtt_string_t *str)
{
    return str->len >= 0 && str->len <= 0xffff;
}

/******************************************************************************
 * lmqtt_fixed_header_t PRIVATE functions
 ******************************************************************************/

static lmqtt_decode_result_t fixed_header_decode(lmqtt_fixed_header_t *header,
    u8 b)
{
    int result = LMQTT_DECODE_ERROR;

    if (header->internal.failed)
        return LMQTT_DECODE_ERROR;

    if (header->internal.bytes_read == 0) {
        int type = b >> 4;
        int flags = b & 0x0f;
        int bad_flags;

        switch (type) {
            case LMQTT_TYPE_PUBREL:
            case LMQTT_TYPE_SUBSCRIBE:
            case LMQTT_TYPE_UNSUBSCRIBE:
                bad_flags = flags != 2;
                break;
            case LMQTT_TYPE_PUBLISH:
                bad_flags = (flags & 6) == 6;
                break;
            default:
                bad_flags = flags != 0;
        }

        if (type < LMQTT_TYPE_MIN || type > LMQTT_TYPE_MAX || bad_flags) {
            result = LMQTT_DECODE_ERROR;
        } else {
            header->type = type;
            header->internal.remain_len_multiplier = 1;
            header->internal.remain_len_accumulator = 0;
            header->internal.remain_len_finished = 0;
            if (type == LMQTT_TYPE_PUBLISH) {
                header->dup = (flags & 8) >> 3;
                header->qos = (flags & 6) >> 1;
                header->retain = flags & 1;
            } else {
                header->dup = 0;
                header->qos = 0;
                header->retain = 0;
            }
            result = LMQTT_DECODE_CONTINUE;
        }
    } else {
        if (header->internal.remain_len_multiplier > 128 * 128 && (b & 128) != 0 ||
                header->internal.remain_len_multiplier > 1 && b == 0 ||
                header->internal.remain_len_finished) {
            result = LMQTT_DECODE_ERROR;
        } else {
            header->internal.remain_len_accumulator += (b & 127) *
                header->internal.remain_len_multiplier;
            header->internal.remain_len_multiplier *= 128;

            if (b & 128) {
                result = LMQTT_DECODE_CONTINUE;
            } else {
                header->remaining_length =
                    header->internal.remain_len_accumulator;
                header->internal.remain_len_finished = 1;
                result = LMQTT_DECODE_FINISHED;
            }
        }
    }

    if (result == LMQTT_DECODE_ERROR)
        header->internal.failed = 1;
    else
        header->internal.bytes_read += 1;
    return result;
}

/******************************************************************************
 * lmqtt_connect_t PRIVATE functions
 ******************************************************************************/

static int connect_calc_remaining_length(lmqtt_connect_t *connect)
{
    return LMQTT_CONNECT_HEADER_SIZE +
        /* client_id is always present in payload */
        LMQTT_STRING_LEN_SIZE + connect->client_id.len +
        string_calc_field_length(&connect->will_topic) +
        string_calc_field_length(&connect->will_message) +
        string_calc_field_length(&connect->user_name) +
        string_calc_field_length(&connect->password);
 }

static lmqtt_encode_result_t connect_build_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    int remain_len_size;
    int res;
    lmqtt_connect_t *connect = value->value;

    assert(sizeof(encode_buffer->buf) >= LMQTT_FIXED_HEADER_MAX_SIZE);

    res = encode_remaining_length(connect_calc_remaining_length(connect),
        encode_buffer->buf + 1, &remain_len_size);
    if (res != LMQTT_ENCODE_FINISHED)
        return LMQTT_ENCODE_ERROR;

    encode_buffer->buf[0] = LMQTT_TYPE_CONNECT << 4;
    encode_buffer->buf_len = 1 + remain_len_size;
    return LMQTT_ENCODE_FINISHED;
}

static lmqtt_encode_result_t connect_encode_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        connect_build_fixed_header, offset, buf, buf_len, bytes_written);
}

static lmqtt_encode_result_t connect_build_variable_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    u8 flags;
    lmqtt_connect_t *connect = value->value;

    assert(sizeof(encode_buffer->buf) >= LMQTT_CONNECT_HEADER_SIZE);

    memcpy(encode_buffer->buf, "\x00\x04MQTT\x04", 7);

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
    encode_buffer->buf[7] = flags;

    encode_buffer->buf[8] = STRING_LEN_BYTE(connect->keep_alive, 1);
    encode_buffer->buf[9] = STRING_LEN_BYTE(connect->keep_alive, 0);
    encode_buffer->buf_len = 10;
    return LMQTT_ENCODE_FINISHED;
}

static lmqtt_encode_result_t connect_encode_variable_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        connect_build_variable_header, offset, buf, buf_len, bytes_written);
}

static lmqtt_encode_result_t connect_encode_payload_client_id(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    lmqtt_connect_t *connect = value->value;

    return string_encode(&connect->client_id, 1, 1, offset, buf, buf_len,
        bytes_written, &encode_buffer->blocking_str);
}

static lmqtt_encode_result_t connect_encode_payload_will_topic(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    lmqtt_connect_t *connect = value->value;

    return string_encode(&connect->will_topic, 1, 0, offset, buf, buf_len,
        bytes_written, &encode_buffer->blocking_str);
}

static lmqtt_encode_result_t connect_encode_payload_will_message(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    lmqtt_connect_t *connect = value->value;

    return string_encode(&connect->will_message, 1, 0, offset, buf, buf_len,
        bytes_written, &encode_buffer->blocking_str);
}

static lmqtt_encode_result_t connect_encode_payload_user_name(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    lmqtt_connect_t *connect = value->value;

    return string_encode(&connect->user_name, 1, 0, offset, buf, buf_len,
        bytes_written, &encode_buffer->blocking_str);
}

static lmqtt_encode_result_t connect_encode_payload_password(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    lmqtt_connect_t *connect = value->value;

    return string_encode(&connect->password, 1, 0, offset, buf, buf_len,
        bytes_written, &encode_buffer->blocking_str);
}

/******************************************************************************
 * lmqtt_connect_t PUBLIC functions
 ******************************************************************************/

int lmqtt_connect_validate(lmqtt_connect_t *connect)
{
    if (!string_validate_field_length(&connect->client_id) ||
            !string_validate_field_length(&connect->will_topic) ||
            !string_validate_field_length(&connect->will_message) ||
            !string_validate_field_length(&connect->user_name) ||
            !string_validate_field_length(&connect->password))
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

/******************************************************************************
 * lmqtt_subscribe_t PRIVATE functions
 ******************************************************************************/

static int subscribe_calc_remaining_length(lmqtt_subscribe_t *subscribe,
    int include_qos)
{
    int i;
    int result = LMQTT_PACKET_ID_SIZE;

    for (i = 0; i < subscribe->count; i++)
        result += subscribe->subscriptions[i].topic.len +
            LMQTT_STRING_LEN_SIZE + (include_qos ? 1 : 0);

    return result;
}

static lmqtt_encode_result_t subscribe_build_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int type, int include_qos)
{
    int res, i, v;
    lmqtt_subscribe_t *subscribe = value->value;

    res = encode_remaining_length(subscribe_calc_remaining_length(subscribe,
        include_qos), encode_buffer->buf + 1, &v);
    if (res != LMQTT_ENCODE_FINISHED)
        return LMQTT_ENCODE_ERROR;

    encode_buffer->buf[0] = type << 4 | 0x02;
    for (i = 0; i < LMQTT_PACKET_ID_SIZE; i++)
        encode_buffer->buf[v + i + 1] = STRING_LEN_BYTE(value->packet_id,
            LMQTT_PACKET_ID_SIZE - i - 1);

    encode_buffer->buf_len = v + LMQTT_PACKET_ID_SIZE + 1;
    return LMQTT_ENCODE_FINISHED;
}

static lmqtt_encode_result_t subscribe_build_header_subscribe(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    return subscribe_build_header(value, encode_buffer,
        LMQTT_TYPE_SUBSCRIBE, 1);
}

static lmqtt_encode_result_t subscribe_encode_header_subscribe(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        subscribe_build_header_subscribe, offset, buf, buf_len, bytes_written);
}

static lmqtt_encode_result_t subscribe_build_header_unsubscribe(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    return subscribe_build_header(value, encode_buffer,
        LMQTT_TYPE_UNSUBSCRIBE, 0);
}

static lmqtt_encode_result_t subscribe_encode_header_unsubscribe(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        subscribe_build_header_unsubscribe, offset, buf, buf_len,
        bytes_written);
}

static lmqtt_encode_result_t subscribe_encode_topic(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    lmqtt_subscribe_t *subscribe = value->value;

    return string_encode(&subscribe->internal.current->topic, 1, 1, offset, buf,
        buf_len, bytes_written, &encode_buffer->blocking_str);
}

static lmqtt_encode_result_t subscribe_build_qos(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    lmqtt_subscribe_t *subscribe = value->value;

    encode_buffer->buf[0] = subscribe->internal.current->qos;
    encode_buffer->buf_len = 1;
    return LMQTT_ENCODE_FINISHED;
}

static lmqtt_encode_result_t subscribe_encode_qos(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        subscribe_build_qos, offset, buf, buf_len, bytes_written);
}

/******************************************************************************
 * lmqtt_subscribe_t PUBLIC functions
 ******************************************************************************/

int lmqtt_subscribe_validate(lmqtt_subscribe_t *subscribe)
{
    int i;
    int cnt = subscribe->count;

    if (cnt == 0 || !subscribe->subscriptions)
        return 0;

    for (i = 0; i < cnt; i++) {
        lmqtt_subscription_t *sub = &subscribe->subscriptions[i];
        if (!string_validate_field_length(&sub->topic))
            return 0;
        if (sub->topic.len == 0 || sub->qos < 0 || sub->qos > 2)
            return 0;
    }

    return 1;
}

/******************************************************************************
 * lmqtt_publish_t PRIVATE functions
 ******************************************************************************/

static int publish_calc_remaining_length(lmqtt_publish_t *publish)
{
    return LMQTT_PACKET_ID_SIZE + LMQTT_STRING_LEN_SIZE + publish->topic.len +
        publish->payload.len;
}

static lmqtt_encode_result_t publish_build_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    int res, i, v;
    u8 type;
    lmqtt_publish_t *publish = value->value;

    res = encode_remaining_length(publish_calc_remaining_length(publish),
        encode_buffer->buf + 1, &v);
    if (res != LMQTT_ENCODE_FINISHED)
        return LMQTT_ENCODE_ERROR;

    type = LMQTT_TYPE_PUBLISH << 4;
    type |= publish->retain ? 0x01 : 0x00;
    type |= (publish->qos & 0x03) << 1;
    type |= publish->internal.encode_count > 0 ? 0x08 : 0x00;
    encode_buffer->buf[0] = type;
    encode_buffer->buf_len = 1 + v;
    return LMQTT_ENCODE_FINISHED;
}

static lmqtt_encode_result_t publish_encode_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        publish_build_fixed_header, offset, buf, buf_len, bytes_written);
}

static lmqtt_encode_result_t publish_encode_topic(lmqtt_store_value_t *value,
    lmqtt_encode_buffer_t *encode_buffer, int offset, u8 *buf, int buf_len,
    int *bytes_written)
{
    lmqtt_publish_t *publish = value->value;

    return string_encode(&publish->topic, 1, 1, offset, buf, buf_len,
        bytes_written, &encode_buffer->blocking_str);
}

static lmqtt_encode_result_t publish_build_packet_id(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    int i;
    lmqtt_publish_t *publish = value->value;

    for (i = 0; i < LMQTT_PACKET_ID_SIZE; i++)
        encode_buffer->buf[i] = STRING_LEN_BYTE(value->packet_id,
            LMQTT_PACKET_ID_SIZE - i - 1);

    encode_buffer->buf_len = LMQTT_PACKET_ID_SIZE;
    return LMQTT_ENCODE_FINISHED;
}

static lmqtt_encode_result_t publish_encode_packet_id(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        publish_build_packet_id, offset, buf, buf_len, bytes_written);
}

static lmqtt_encode_result_t publish_encode_payload(lmqtt_store_value_t *value,
    lmqtt_encode_buffer_t *encode_buffer, int offset, u8 *buf, int buf_len,
    int *bytes_written)
{
    lmqtt_publish_t *publish = value->value;
    return string_encode(&publish->payload, 0, 0, offset, buf, buf_len,
        bytes_written, &encode_buffer->blocking_str);
}

static lmqtt_encode_result_t publish_build_pubrel(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    int i;
    lmqtt_publish_t *publish = value->value;

    encode_buffer->buf[0] = (LMQTT_TYPE_PUBREL << 4) | 0x02;
    encode_buffer->buf[1] = 0x02;

    for (i = 0; i < LMQTT_PACKET_ID_SIZE; i++)
        encode_buffer->buf[i + 2] = STRING_LEN_BYTE(value->packet_id,
            LMQTT_PACKET_ID_SIZE - i - 1);

    encode_buffer->buf_len = LMQTT_PACKET_ID_SIZE + 2;
    return LMQTT_ENCODE_FINISHED;
}

static lmqtt_encode_result_t publish_encode_pubrel(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        publish_build_pubrel, offset, buf, buf_len, bytes_written);
}

/******************************************************************************
 * lmqtt_publish_t PUBLIC functions
 ******************************************************************************/

int lmqtt_publish_validate(lmqtt_publish_t *publish)
{
    return string_validate_field_length(&publish->topic) &&
        publish->topic.len > 0 && publish->qos >= 0 && publish->qos <= 2 &&
        publish_calc_remaining_length(publish) <= 0xfffffff;
}

/******************************************************************************
 * (pingreq) PUBLIC functions
 ******************************************************************************/

static lmqtt_encode_result_t pingreq_build(lmqtt_store_value_t *value,
    lmqtt_encode_buffer_t *encode_buffer)
{
    assert(sizeof(encode_buffer->buf) >= 2);

    encode_buffer->buf[0] = LMQTT_TYPE_PINGREQ << 4;
    encode_buffer->buf[1] = 0;
    encode_buffer->buf_len = 2;
    return LMQTT_ENCODE_FINISHED;
}

static lmqtt_encode_result_t pingreq_encode_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value, pingreq_build, offset,
        buf, buf_len, bytes_written);
}

/******************************************************************************
 * (disconnect) PUBLIC functions
 ******************************************************************************/

static lmqtt_encode_result_t disconnect_build(lmqtt_store_value_t *value,
    lmqtt_encode_buffer_t *encode_buffer)
{
    assert(sizeof(encode_buffer->buf) >= 2);

    encode_buffer->buf[0] = LMQTT_TYPE_DISCONNECT << 4;
    encode_buffer->buf[1] = 0;
    encode_buffer->buf_len = 2;
    return LMQTT_ENCODE_FINISHED;
}

static lmqtt_encode_result_t disconnect_encode_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value, disconnect_build, offset,
        buf, buf_len, bytes_written);
}

/******************************************************************************
 * lmqtt_tx_buffer_t PRIVATE functions
 ******************************************************************************/

static lmqtt_encoder_t tx_buffer_finder_connect(lmqtt_tx_buffer_t *tx_buffer,
    lmqtt_store_value_t *value)
{
    switch (tx_buffer->internal.pos) {
        case 0: return &connect_encode_fixed_header;
        case 1: return &connect_encode_variable_header;
        case 2: return &connect_encode_payload_client_id;
        case 3: return &connect_encode_payload_will_topic;
        case 4: return &connect_encode_payload_will_message;
        case 5: return &connect_encode_payload_user_name;
        case 6: return &connect_encode_payload_password;
    }
    return 0;
}

static lmqtt_encoder_t tx_buffer_finder_subscribe(lmqtt_tx_buffer_t *tx_buffer,
    lmqtt_store_value_t *value)
{
    lmqtt_subscribe_t *subscribe = value->value;
    int p = tx_buffer->internal.pos;

    if (p == 0) {
        subscribe->internal.current = 0;
        return &subscribe_encode_header_subscribe;
    }

    p -= 1;
    if (p >= 0 && p < subscribe->count * 2) {
        subscribe->internal.current = &subscribe->subscriptions[p / 2];
        return p % 2 == 0 ? &subscribe_encode_topic : &subscribe_encode_qos;
    }

    return 0;
}

static lmqtt_encoder_t tx_buffer_finder_unsubscribe(
    lmqtt_tx_buffer_t *tx_buffer, lmqtt_store_value_t *value)
{
    lmqtt_subscribe_t *subscribe = value->value;
    int p = tx_buffer->internal.pos;

    if (p == 0) {
        subscribe->internal.current = 0;
        return &subscribe_encode_header_unsubscribe;
    }

    p -= 1;
    if (p >= 0 && p < subscribe->count) {
        subscribe->internal.current = &subscribe->subscriptions[p];
        return &subscribe_encode_topic;
    }

    return 0;
}

static lmqtt_encoder_t tx_buffer_finder_publish(lmqtt_tx_buffer_t *tx_buffer,
    lmqtt_store_value_t *value)
{
    lmqtt_publish_t *publish = value->value;

    switch (tx_buffer->internal.pos) {
        case 0: return &publish_encode_fixed_header;
        case 1: return &publish_encode_topic;
        case 2: return &publish_encode_packet_id;
        case 3: return &publish_encode_payload;
    }

    publish->internal.encode_count++;
    return 0;
}

static lmqtt_encoder_t tx_buffer_finder_pubrel(lmqtt_tx_buffer_t *tx_buffer,
    lmqtt_store_value_t *value)
{
    return tx_buffer->internal.pos == 0 ? &publish_encode_pubrel : 0;
}

static lmqtt_encoder_t tx_buffer_finder_pingreq(lmqtt_tx_buffer_t *tx_buffer,
    lmqtt_store_value_t *value)
{
    return tx_buffer->internal.pos == 0 ? &pingreq_encode_fixed_header : 0;
}

static lmqtt_encoder_t tx_buffer_finder_disconnect(lmqtt_tx_buffer_t *tx_buffer,
    lmqtt_store_value_t *value)
{
    return tx_buffer->internal.pos == 0 ? &disconnect_encode_fixed_header : 0;
}

/* Enable mocking of tx_buffer_finder_by_class() in test-cases. */
#ifndef TX_BUFFER_FINDER_BY_CLASS
    #define TX_BUFFER_FINDER_BY_CLASS tx_buffer_finder_by_class
#endif

static lmqtt_encoder_finder_t tx_buffer_finder_by_class(lmqtt_class_t class)
{
    switch (class) {
        case LMQTT_CLASS_CONNECT: return &tx_buffer_finder_connect;
        case LMQTT_CLASS_SUBSCRIBE: return &tx_buffer_finder_subscribe;
        case LMQTT_CLASS_UNSUBSCRIBE: return &tx_buffer_finder_unsubscribe;
        case LMQTT_CLASS_PUBLISH_0:
        case LMQTT_CLASS_PUBLISH_1:
        case LMQTT_CLASS_PUBLISH_2: return &tx_buffer_finder_publish;
        case LMQTT_CLASS_PUBREL: return &tx_buffer_finder_pubrel;
        case LMQTT_CLASS_PINGREQ: return &tx_buffer_finder_pingreq;
        case LMQTT_CLASS_DISCONNECT: return &tx_buffer_finder_disconnect;
    }
    return NULL;
}

/******************************************************************************
 * lmqtt_tx_buffer_t PUBLIC functions
 ******************************************************************************/

void lmqtt_tx_buffer_reset(lmqtt_tx_buffer_t *state)
{
    state->closed = 0;
    memset(&state->internal, 0, sizeof(state->internal));
}

void lmqtt_tx_buffer_finish(lmqtt_tx_buffer_t *state)
{
    state->closed = 1;
}

lmqtt_io_result_t lmqtt_tx_buffer_encode(lmqtt_tx_buffer_t *state, u8 *buf,
    int buf_len, int *bytes_written)
{
    int offset = 0;
    *bytes_written = 0;
    int class;
    lmqtt_store_value_t value;

    while (!state->closed && lmqtt_store_peek(state->store, &class, &value)) {
        lmqtt_encoder_finder_t finder = TX_BUFFER_FINDER_BY_CLASS(class);

        if (!finder)
            return LMQTT_IO_ERROR;

        while (1) {
            int result;
            int cur_bytes;
            lmqtt_encoder_t encoder = finder(state, &value);

            if (!encoder) {
                if (class == LMQTT_CLASS_DISCONNECT) {
                    lmqtt_store_drop_current(state->store);
                    lmqtt_tx_buffer_finish(state);
                    break;
                } else if (class == LMQTT_CLASS_PUBLISH_0) {
                    lmqtt_store_drop_current(state->store);
                    value.callback(value.callback_data, value.value);
                } else {
                    lmqtt_store_mark_current(state->store);
                }
                lmqtt_tx_buffer_reset(state);
                break;
            }

            result = encoder(&value, &state->internal.buffer,
                state->internal.offset, buf + offset, buf_len - offset,
                &cur_bytes);
            if (result == LMQTT_ENCODE_WOULD_BLOCK)
                return LMQTT_IO_AGAIN;
            if (result == LMQTT_ENCODE_CONTINUE)
                state->internal.offset += cur_bytes;
            if (result == LMQTT_ENCODE_CONTINUE || result == LMQTT_ENCODE_FINISHED)
                *bytes_written += cur_bytes;
            if (result == LMQTT_ENCODE_CONTINUE)
                return LMQTT_IO_SUCCESS;
            if (result == LMQTT_ENCODE_ERROR)
                return LMQTT_IO_ERROR;

            offset += cur_bytes;
            state->internal.pos += 1;
            state->internal.offset = 0;
        }
    }

    return *bytes_written > 0 || state->closed ?
        LMQTT_IO_SUCCESS : LMQTT_IO_AGAIN;
}

lmqtt_string_t *lmqtt_tx_buffer_get_blocking_str(lmqtt_tx_buffer_t *state)
{
    return state->internal.buffer.blocking_str;
}

/******************************************************************************
 * lmqtt_rx_buffer_t PRIVATE functions
 ******************************************************************************/

static lmqtt_decode_result_t rx_buffer_decode_connack(lmqtt_rx_buffer_t *state,
    u8 b)
{
    lmqtt_connect_t *connect = (lmqtt_connect_t *) state->internal.value.value;

    switch (state->internal.remain_buf_pos) {
        case 0:
            if (b & ~1)
                return LMQTT_DECODE_ERROR;
            connect->response.session_present = b;
            return LMQTT_DECODE_CONTINUE;
        case 1:
            if (b > LMQTT_CONNACK_RC_MAX)
                return LMQTT_DECODE_ERROR;
            connect->response.return_code = b;
            return LMQTT_DECODE_FINISHED;
        default:
            return LMQTT_DECODE_ERROR;
    }
}

static lmqtt_decode_result_t rx_buffer_decode_publish(lmqtt_rx_buffer_t *state,
    u8 b)
{
    int rem_len = state->internal.header.remaining_length;
    int rem_pos = state->internal.remain_buf_pos + 1;
    static const int s_len = LMQTT_STRING_LEN_SIZE;
    static const int p_len = LMQTT_PACKET_ID_SIZE;
    lmqtt_publish_t *publish;

    if (rem_pos <= s_len) {
        state->internal.topic_len |= b << ((s_len - rem_pos) * 8);
        if (rem_pos == s_len && (state->internal.topic_len == 0 ||
                state->internal.topic_len + s_len + p_len > rem_len))
            return LMQTT_DECODE_ERROR;
    } else {
        int p_start = s_len + state->internal.topic_len;
        if (rem_pos >= p_start && rem_pos <= p_start + p_len)
            state->internal.packet_id |= b << ((p_len - rem_pos + p_start) * 8);
    }

    if (rem_len > rem_pos)
        return LMQTT_DECODE_CONTINUE;

    publish = &state->internal.publish;
    publish->qos = state->internal.header.qos;
    publish->retain = state->internal.header.retain;

    if (state->on_publish)
        state->on_publish(state->on_publish_data, &state->internal.publish);
    return LMQTT_DECODE_FINISHED;
}

static lmqtt_decode_result_t rx_buffer_decode_suback(lmqtt_rx_buffer_t *state,
    u8 b)
{
    lmqtt_subscribe_t *subscribe =
        (lmqtt_subscribe_t *) state->internal.value.value;

    int pos = state->internal.remain_buf_pos - LMQTT_PACKET_ID_SIZE;

    if (pos == 0) {
        int len = state->internal.header.remaining_length - LMQTT_PACKET_ID_SIZE;
        if (len != subscribe->count)
            return LMQTT_DECODE_ERROR;
    }

    if (b > 2 && b != 0x80)
        return LMQTT_DECODE_ERROR;

    subscribe->subscriptions[pos].return_code = b;
    return pos + 1 >= subscribe->count ?
        LMQTT_DECODE_FINISHED : LMQTT_DECODE_CONTINUE;
}

/*
 * Enable mocking of rx_buffer_call_callback() and rx_buffer_decode_type() in
 * test-cases.
 */
#ifndef RX_BUFFER_CALL_CALLBACK
    #define RX_BUFFER_CALL_CALLBACK rx_buffer_call_callback
#endif
#ifndef RX_BUFFER_DECODE_TYPE
    #define RX_BUFFER_DECODE_TYPE rx_buffer_decode_type
#endif

/*
 * Return: 1 on success, 0 on failure
 */
static int rx_buffer_call_callback(lmqtt_rx_buffer_t *state)
{
    lmqtt_store_value_t *value = &state->internal.value;

    if (value->callback)
        return value->callback(value->callback_data, value->value);

    return 0;
}

static lmqtt_decode_result_t rx_buffer_decode_type(lmqtt_rx_buffer_t *state,
    u8 b)
{
    if (!state->internal.decoder->decode_byte)
        return LMQTT_DECODE_ERROR;

    return state->internal.decoder->decode_byte(state, b);
}

static lmqtt_io_result_t rx_buffer_fail(lmqtt_rx_buffer_t *state)
{
    state->internal.failed = 1;
    return LMQTT_IO_ERROR;
}

static int rx_buffer_pop_packet(lmqtt_rx_buffer_t *state, u16 packet_id)
{
    return lmqtt_store_pop_marked_by(state->store,
        state->internal.decoder->class, packet_id, &state->internal.value);
}

static int rx_buffer_is_packet_finished(lmqtt_rx_buffer_t *state)
{
    return state->internal.header_finished && state->internal.remain_buf_pos >=
        state->internal.header.remaining_length;
}

static int rx_buffer_finish_packet(lmqtt_rx_buffer_t *state)
{
    int result;

    if (state->internal.decoder->class == LMQTT_CLASS_PUBLISH_2)
        result = lmqtt_store_append(state->store, LMQTT_CLASS_PUBREL,
            &state->internal.value);
    else
        result = RX_BUFFER_CALL_CALLBACK(state);

    lmqtt_rx_buffer_reset(state);

    return result;
}

static int rx_buffer_pop_packet_without_id(lmqtt_rx_buffer_t *state)
{
    return rx_buffer_pop_packet(state, 0);
}

static int rx_buffer_pop_packet_with_id(lmqtt_rx_buffer_t *state)
{
    return rx_buffer_pop_packet(state, state->internal.packet_id);
}

static int rx_buffer_pop_packet_ignore(lmqtt_rx_buffer_t *state)
{
    return 1;
}

static int rx_buffer_decode_remaining_without_id(lmqtt_rx_buffer_t *state, u8 b)
{
    int rem_pos = state->internal.remain_buf_pos + 1;
    int rem_len = state->internal.header.remaining_length;

    int res = RX_BUFFER_DECODE_TYPE(state, b);

    if (res == LMQTT_DECODE_ERROR)
        return 0;
    if (res != LMQTT_DECODE_FINISHED && rem_pos >= rem_len)
        return 0;
    if (res == LMQTT_DECODE_FINISHED && rem_pos < rem_len)
        return 0;

    return 1;
}

static int rx_buffer_decode_remaining_with_id(lmqtt_rx_buffer_t *state, u8 b)
{
    int rem_pos = state->internal.remain_buf_pos + 1;
    static const int p_len = LMQTT_PACKET_ID_SIZE;

    if (rem_pos > p_len)
        return rx_buffer_decode_remaining_without_id(state, b);

    state->internal.packet_id |= b << ((p_len - rem_pos) * 8);

    if (rem_pos == p_len && !state->internal.decoder->pop_packet_with_id(state))
        return 0;

    return 1;
}

static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_connack = {
    2,
    LMQTT_CLASS_CONNECT,
    &rx_buffer_pop_packet_without_id,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_decode_remaining_without_id,
    &rx_buffer_decode_connack
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_publish = {
    5,
    0, /* never used */
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_decode_remaining_without_id,
    &rx_buffer_decode_publish
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_puback = {
    2,
    LMQTT_CLASS_PUBLISH_1,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pop_packet_with_id,
    &rx_buffer_decode_remaining_with_id,
    NULL
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_pubrec = {
    2,
    LMQTT_CLASS_PUBLISH_2,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pop_packet_with_id,
    &rx_buffer_decode_remaining_with_id,
    NULL
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_pubrel = {
    2,
    0, /* never used */
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_decode_remaining_with_id,
    NULL
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_pubcomp = {
    2,
    LMQTT_CLASS_PUBREL,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pop_packet_with_id,
    &rx_buffer_decode_remaining_with_id,
    NULL
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_suback = {
    3,
    LMQTT_CLASS_SUBSCRIBE,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pop_packet_with_id,
    &rx_buffer_decode_remaining_with_id,
    &rx_buffer_decode_suback
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_unsuback = {
    2,
    LMQTT_CLASS_UNSUBSCRIBE,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pop_packet_with_id,
    &rx_buffer_decode_remaining_with_id,
    NULL
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_pingresp = {
    0,
    LMQTT_CLASS_PINGREQ,
    &rx_buffer_pop_packet_without_id,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_decode_remaining_without_id,
    NULL
};

static struct _lmqtt_rx_buffer_decoder_t const
        *rx_buffer_decoders[LMQTT_TYPE_MAX + 1] = {
    NULL,   /* 0 */
    NULL,   /* CONNECT */
    &rx_buffer_decoder_connack,
    &rx_buffer_decoder_publish,
    &rx_buffer_decoder_puback,
    &rx_buffer_decoder_pubrec,
    &rx_buffer_decoder_pubrel,
    &rx_buffer_decoder_pubcomp,
    NULL,   /* SUBSCRIBE */
    &rx_buffer_decoder_suback,
    NULL,   /* UNSUBSCRIBE */
    &rx_buffer_decoder_unsuback,
    NULL,   /* PINGREQ */
    &rx_buffer_decoder_pingresp,
    NULL    /* DISCONNECT */
};

/******************************************************************************
 * lmqtt_rx_buffer_t PUBLIC functions
 ******************************************************************************/

void lmqtt_rx_buffer_reset(lmqtt_rx_buffer_t *state)
{
    memset(&state->internal, 0, sizeof(state->internal));
}

void lmqtt_rx_buffer_finish(lmqtt_rx_buffer_t *state)
{
    RX_BUFFER_CALL_CALLBACK(state);
}

/*
 * TODO: lmqtt_rx_buffer_decode() should be able to handle cases where the buffer
 * cannot be completely read (for example, if a callback which is being invoked
 * to write the incoming data to a file would block) and return
 * LMQTT_IO_AGAIN. Otherwise it should return LMQTT_IO_SUCCESS, even if
 * the incoming packet is not yet complete. (That may look confusing. Should we
 * have different return codes for lmqtt_rx_buffer_decode() and the other decoding
 * functions?)
 */
lmqtt_io_result_t lmqtt_rx_buffer_decode(lmqtt_rx_buffer_t *state, u8 *buf,
    int buf_len, int *bytes_read)
{
    int i;

    *bytes_read = 0;

    if (state->internal.failed)
        return LMQTT_IO_ERROR;

    for (i = 0; i < buf_len; i++) {
        *bytes_read += 1;

        if (!state->internal.header_finished) {
            int rem_len;
            int res = fixed_header_decode(&state->internal.header, buf[i]);

            if (res == LMQTT_DECODE_ERROR)
                return rx_buffer_fail(state);
            if (res != LMQTT_DECODE_FINISHED)
                continue;

            state->internal.header_finished = 1;
            state->internal.decoder =
                rx_buffer_decoders[state->internal.header.type];
            rem_len = state->internal.header.remaining_length;

            if (!state->internal.decoder)
                return rx_buffer_fail(state);

            if (rem_len < state->internal.decoder->min_length)
                return rx_buffer_fail(state);

            if (!state->internal.decoder->pop_packet_without_id(state))
                return rx_buffer_fail(state);
        } else {
            if (!state->internal.decoder->decode_remaining(state, buf[i]))
                return rx_buffer_fail(state);

            state->internal.remain_buf_pos += 1;
        }

        if (rx_buffer_is_packet_finished(state))
            rx_buffer_finish_packet(state);
    }

    /* Even when processing a CONNACK this will touch the correct store, because
       lmqtt_client_t will change the current store during the callback, which
       is called from state->internal.decoder->decode_remaining() */
    if (*bytes_read > 0)
        lmqtt_store_touch(state->store);
    return LMQTT_IO_SUCCESS;
}
