#include <lightmqtt/packet.h>
#include <lightmqtt/types.h>
#include <string.h>
#include <assert.h>

#define LMQTT_FLAG_CLEAN_SESSION 0x02
#define LMQTT_FLAG_WILL_FLAG 0x04
#define LMQTT_FLAG_WILL_RETAIN 0x20
#define LMQTT_FLAG_PASSWORD_FLAG 0x40
#define LMQTT_FLAG_USER_NAME_FLAG 0x80
#define LMQTT_OFFSET_FLAG_QOS 3

#define LMQTT_STRING_LEN_SIZE 2
#define LMQTT_PACKET_ID_SIZE 2
#define LMQTT_REMAINING_LENGTH_MAX_SIZE 4

#define STRING_LEN_BYTE(val, num) (((val) >> ((num) * 8)) & 0xff)

/******************************************************************************
 * GENERAL PRIVATE functions
 ******************************************************************************/

/* caller must guarantee buf is at least 4-bytes long! */
LMQTT_STATIC lmqtt_encode_result_t encode_remaining_length(long len,
    unsigned char *buf, size_t *bytes_written)
{
    size_t pos;

    if (len < 0 || len > 0x0fffffff)
        return LMQTT_ENCODE_ERROR;

    pos = 0;
    do {
        unsigned char b = len % 128;
        len /= 128;
        buf[pos++] = len > 0 ? b | 0x80 : b;
    } while (len > 0);

    *bytes_written = pos;
    return LMQTT_ENCODE_FINISHED;
}

LMQTT_STATIC int kind_expects_response(lmqtt_kind_t kind)
{
    return kind != LMQTT_KIND_PUBLISH_0 && kind != LMQTT_KIND_PUBACK &&
        kind != LMQTT_KIND_PUBREC && kind != LMQTT_KIND_PUBCOMP &&
        kind != LMQTT_KIND_DISCONNECT;
}

/******************************************************************************
 * lmqtt_id_list_t PUBLIC functions
 ******************************************************************************/

int lmqtt_id_set_clear(lmqtt_id_set_t *id_set)
{
    id_set->count = 0;
}

int lmqtt_id_set_contains(lmqtt_id_set_t *id_set, lmqtt_packet_id_t id)
{
    int i;

    for (i = 0; i < id_set->count; i++) {
        if (id_set->items[i] == id)
            return 1;
    }

    return 0;
}

int lmqtt_id_set_put(lmqtt_id_set_t *id_set, lmqtt_packet_id_t id)
{
    int i;

    if (id_set->count >= id_set->capacity)
        return 0;

    for (i = 0; i < id_set->count; i++) {
        if (id_set->items[i] == id)
            return 0;
    }

    id_set->items[id_set->count++] = id;
    return 1;
}

int lmqtt_id_set_remove(lmqtt_id_set_t *id_set, lmqtt_packet_id_t id)
{
    int i;

    for (i = 0; i < id_set->count; i++) {
        if (id_set->items[i] == id) {
            memmove(&id_set->items[i], &id_set->items[i + 1],
                sizeof(&id_set->items[0]) * (id_set->count - i - 1));
            id_set->count--;
            return 1;
        }
    }

    return 0;
}

/******************************************************************************
 * lmqtt_encode_buffer_t PRIVATE functions
 ******************************************************************************/

LMQTT_STATIC lmqtt_encode_result_t encode_buffer_encode(
    lmqtt_encode_buffer_t *encode_buffer, lmqtt_store_value_t *value,
    encode_buffer_builder_t builder, size_t offset, unsigned char *buf,
    size_t buf_len, size_t *bytes_written)
{
    size_t cnt;
    int result;

    assert(buf_len >= 0);

    if (!encode_buffer->encoded) {
        if (builder(value, encode_buffer) != LMQTT_ENCODE_FINISHED) {
            *bytes_written = 0;
            return LMQTT_ENCODE_ERROR;
        }
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

LMQTT_STATIC lmqtt_encode_result_t encode_buffer_encode_packet_id(
    lmqtt_encode_buffer_t *encode_buffer, int type, long remaining_length,
    lmqtt_packet_id_t packet_id)
{
    int res, i;
    size_t v;

    assert(sizeof(encode_buffer->buf) >= 5);

    res = encode_remaining_length(remaining_length, encode_buffer->buf + 1, &v);
    if (res != LMQTT_ENCODE_FINISHED)
        return LMQTT_ENCODE_ERROR;

    assert(sizeof(encode_buffer->buf) >= v + LMQTT_PACKET_ID_SIZE + 1);

    encode_buffer->buf[0] = type;
    for (i = 0; i < LMQTT_PACKET_ID_SIZE; i++)
        encode_buffer->buf[v + i + 1] = STRING_LEN_BYTE(packet_id,
            LMQTT_PACKET_ID_SIZE - i - 1);

    encode_buffer->buf_len = v + LMQTT_PACKET_ID_SIZE + 1;
    return LMQTT_ENCODE_FINISHED;
}

/******************************************************************************
 * lmqtt_string_t PRIVATE functions
 ******************************************************************************/

LMQTT_STATIC lmqtt_io_result_t string_fetch(lmqtt_string_t *str,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
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
    return LMQTT_IO_SUCCESS;
}

LMQTT_STATIC lmqtt_encode_result_t string_encode(lmqtt_string_t *str,
    int encode_len, int encode_if_empty, size_t offset, unsigned char *buf,
    size_t buf_len, size_t *bytes_written, lmqtt_string_t **blocking_str)
{
    long len = str->len;
    int result;
    size_t pos = 0;
    size_t offset_str = offset;
    int i;

    size_t read_cnt;
    int read_res;
    long remaining;

    *bytes_written = 0;
    *blocking_str = NULL;

    if (len == 0 && !encode_if_empty)
        return LMQTT_ENCODE_FINISHED;

    if (buf_len == 0)
        return LMQTT_ENCODE_CONTINUE;

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

    len -= (long) offset_str;
    remaining = str->len - (long) offset_str;

    if (len > (long) (buf_len - pos))
        len = (long) (buf_len - pos);

    read_res = string_fetch(str, offset_str, buf + pos, (size_t) len,
        &read_cnt);
    *bytes_written += read_cnt;
    assert((long) read_cnt <= remaining);

    if (read_res == LMQTT_IO_WOULD_BLOCK && read_cnt == 0) {
        *blocking_str = str;
        result = LMQTT_ENCODE_WOULD_BLOCK;
    } else if (read_res == LMQTT_IO_SUCCESS && (long) read_cnt >= remaining) {
        result = LMQTT_ENCODE_FINISHED;
    } else if (read_res == LMQTT_IO_SUCCESS && read_cnt > 0) {
        result = LMQTT_ENCODE_CONTINUE;
    } else {
        result = LMQTT_ENCODE_ERROR;
    }

    return result;
}

LMQTT_STATIC lmqtt_decode_result_t string_put(lmqtt_string_t *str,
    unsigned char *buf, size_t buf_len, size_t *bytes_written,
    lmqtt_string_t **blocking_str)
{
    *bytes_written = 0;
    *blocking_str = NULL;

    if (str->internal.pos + buf_len > str->len)
        return LMQTT_DECODE_ERROR;

    if (str->write) {
        switch(str->write(str->data, buf, buf_len, bytes_written)) {
            case LMQTT_IO_SUCCESS:
                str->internal.pos += *bytes_written;
                break;
            case LMQTT_IO_WOULD_BLOCK:
                *blocking_str = str;
                return LMQTT_DECODE_WOULD_BLOCK;
            default:
                return LMQTT_DECODE_ERROR;
        }
    } else {
        memcpy(&str->buf[str->internal.pos], buf, buf_len);
        *bytes_written = buf_len;
        str->internal.pos += buf_len;
    }

    return str->internal.pos < str->len ? LMQTT_DECODE_CONTINUE :
        LMQTT_DECODE_FINISHED;
}

LMQTT_STATIC long string_calc_field_length(lmqtt_string_t *str)
{
    return str->len > 0 ? LMQTT_STRING_LEN_SIZE + str->len : 0;
}

LMQTT_STATIC int string_validate_field_length(lmqtt_string_t *str)
{
    return str->len >= 0 && str->len <= 0xffff;
}

/******************************************************************************
 * lmqtt_fixed_header_t PRIVATE functions
 ******************************************************************************/

LMQTT_STATIC lmqtt_decode_result_t fixed_header_decode(
    lmqtt_fixed_header_t *header, unsigned char b)
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
                bad_flags = (flags & 6) == 6 || (flags & 14) == 8;
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

#define LMQTT_CONNECT_HEADER_SIZE 10

LMQTT_STATIC long connect_calc_remaining_length(lmqtt_connect_t *connect)
{
    return LMQTT_CONNECT_HEADER_SIZE +
        /* client_id is always present in payload */
        LMQTT_STRING_LEN_SIZE + connect->client_id.len +
        string_calc_field_length(&connect->will_topic) +
        string_calc_field_length(&connect->will_message) +
        string_calc_field_length(&connect->user_name) +
        string_calc_field_length(&connect->password);
 }

LMQTT_STATIC lmqtt_encode_result_t connect_build_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    size_t remain_len_size;
    int res;
    lmqtt_connect_t *connect = value->value;

    assert(sizeof(encode_buffer->buf) >= LMQTT_REMAINING_LENGTH_MAX_SIZE + 1);

    res = encode_remaining_length(connect_calc_remaining_length(connect),
        encode_buffer->buf + 1, &remain_len_size);
    if (res != LMQTT_ENCODE_FINISHED)
        return LMQTT_ENCODE_ERROR;

    encode_buffer->buf[0] = LMQTT_TYPE_CONNECT << 4;
    encode_buffer->buf_len = 1 + remain_len_size;
    return LMQTT_ENCODE_FINISHED;
}

LMQTT_STATIC lmqtt_encode_result_t connect_encode_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        connect_build_fixed_header, offset, buf, buf_len, bytes_written);
}

LMQTT_STATIC lmqtt_encode_result_t connect_build_variable_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    unsigned char flags;
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

LMQTT_STATIC lmqtt_encode_result_t connect_encode_variable_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        connect_build_variable_header, offset, buf, buf_len, bytes_written);
}

LMQTT_STATIC lmqtt_encode_result_t connect_encode_payload_client_id(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    lmqtt_connect_t *connect = value->value;

    return string_encode(&connect->client_id, 1, 1, offset, buf, buf_len,
        bytes_written, &encode_buffer->blocking_str);
}

LMQTT_STATIC lmqtt_encode_result_t connect_encode_payload_will_topic(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    lmqtt_connect_t *connect = value->value;

    return string_encode(&connect->will_topic, 1, 0, offset, buf, buf_len,
        bytes_written, &encode_buffer->blocking_str);
}

LMQTT_STATIC lmqtt_encode_result_t connect_encode_payload_will_message(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    lmqtt_connect_t *connect = value->value;

    return string_encode(&connect->will_message, 1, 0, offset, buf, buf_len,
        bytes_written, &encode_buffer->blocking_str);
}

LMQTT_STATIC lmqtt_encode_result_t connect_encode_payload_user_name(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    lmqtt_connect_t *connect = value->value;

    return string_encode(&connect->user_name, 1, 0, offset, buf, buf_len,
        bytes_written, &encode_buffer->blocking_str);
}

LMQTT_STATIC lmqtt_encode_result_t connect_encode_payload_password(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
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

LMQTT_STATIC long subscribe_calc_remaining_length(lmqtt_subscribe_t *subscribe,
    int include_qos)
{
    int i;
    long result = LMQTT_PACKET_ID_SIZE;

    for (i = 0; i < subscribe->count; i++)
        result += subscribe->subscriptions[i].topic.len +
            LMQTT_STRING_LEN_SIZE + (include_qos ? 1 : 0);

    return result;
}

LMQTT_STATIC lmqtt_encode_result_t subscribe_build_header_subscribe(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    lmqtt_subscribe_t *subscribe = value->value;

    return encode_buffer_encode_packet_id(
        encode_buffer, (LMQTT_TYPE_SUBSCRIBE << 4) | 0x02,
        subscribe_calc_remaining_length(subscribe, 1), value->packet_id);
}

LMQTT_STATIC lmqtt_encode_result_t subscribe_encode_header_subscribe(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        &subscribe_build_header_subscribe, offset, buf, buf_len, bytes_written);
}

LMQTT_STATIC lmqtt_encode_result_t subscribe_build_header_unsubscribe(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    lmqtt_subscribe_t *subscribe = value->value;

    return encode_buffer_encode_packet_id(
        encode_buffer, (LMQTT_TYPE_UNSUBSCRIBE << 4) | 0x02,
        subscribe_calc_remaining_length(subscribe, 0), value->packet_id);
}

LMQTT_STATIC lmqtt_encode_result_t subscribe_encode_header_unsubscribe(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        &subscribe_build_header_unsubscribe, offset, buf, buf_len,
        bytes_written);
}

LMQTT_STATIC lmqtt_encode_result_t subscribe_encode_topic(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    lmqtt_subscribe_t *subscribe = value->value;

    return string_encode(&subscribe->internal.current->topic, 1, 1, offset, buf,
        buf_len, bytes_written, &encode_buffer->blocking_str);
}

LMQTT_STATIC lmqtt_encode_result_t subscribe_build_qos(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    lmqtt_subscribe_t *subscribe = value->value;

    encode_buffer->buf[0] = subscribe->internal.current->qos;
    encode_buffer->buf_len = 1;
    return LMQTT_ENCODE_FINISHED;
}

LMQTT_STATIC lmqtt_encode_result_t subscribe_encode_qos(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
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

LMQTT_STATIC long publish_calc_remaining_length(lmqtt_publish_t *publish)
{
    return LMQTT_STRING_LEN_SIZE + (long) publish->topic.len +
        (publish->qos == 0 ? 0 : LMQTT_PACKET_ID_SIZE) +
        (long) publish->payload.len;
}

LMQTT_STATIC lmqtt_encode_result_t publish_build_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    int res, i;
    size_t v;
    unsigned char type;
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

LMQTT_STATIC lmqtt_encode_result_t publish_encode_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        publish_build_fixed_header, offset, buf, buf_len, bytes_written);
}

LMQTT_STATIC lmqtt_encode_result_t publish_encode_topic(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    lmqtt_publish_t *publish = value->value;

    return string_encode(&publish->topic, 1, 1, offset, buf, buf_len,
        bytes_written, &encode_buffer->blocking_str);
}

LMQTT_STATIC lmqtt_encode_result_t publish_build_packet_id(
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

LMQTT_STATIC lmqtt_encode_result_t publish_encode_packet_id(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value,
        publish_build_packet_id, offset, buf, buf_len, bytes_written);
}

LMQTT_STATIC lmqtt_encode_result_t publish_encode_payload(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    lmqtt_publish_t *publish = value->value;
    return string_encode(&publish->payload, 0, 0, offset, buf, buf_len,
        bytes_written, &encode_buffer->blocking_str);
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
 * (puback) PUBLIC functions
 ******************************************************************************/

LMQTT_STATIC lmqtt_encode_result_t puback_build(lmqtt_store_value_t *value,
    lmqtt_encode_buffer_t *encode_buffer)
{
    return encode_buffer_encode_packet_id(encode_buffer,
        LMQTT_TYPE_PUBACK << 4, LMQTT_PACKET_ID_SIZE, value->packet_id);
}

LMQTT_STATIC lmqtt_encode_result_t puback_encode_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value, &puback_build, offset,
        buf, buf_len, bytes_written);
}

/******************************************************************************
 * (pubrec) PUBLIC functions
 ******************************************************************************/

LMQTT_STATIC lmqtt_encode_result_t pubrec_build(lmqtt_store_value_t *value,
    lmqtt_encode_buffer_t *encode_buffer)
{
    return encode_buffer_encode_packet_id(encode_buffer,
        LMQTT_TYPE_PUBREC << 4, LMQTT_PACKET_ID_SIZE, value->packet_id);
}

LMQTT_STATIC lmqtt_encode_result_t pubrec_encode_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value, &pubrec_build, offset,
        buf, buf_len, bytes_written);
}

/******************************************************************************
 * (pubrel) PUBLIC functions
 ******************************************************************************/

LMQTT_STATIC lmqtt_encode_result_t pubrel_build(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer)
{
    return encode_buffer_encode_packet_id(encode_buffer, (LMQTT_TYPE_PUBREL <<
        4) | 0x02, LMQTT_PACKET_ID_SIZE, value->packet_id);
}

LMQTT_STATIC lmqtt_encode_result_t pubrel_encode_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value, &pubrel_build, offset,
        buf, buf_len, bytes_written);
}

/******************************************************************************
 * (pubcomp) PUBLIC functions
 ******************************************************************************/

LMQTT_STATIC lmqtt_encode_result_t pubcomp_build(lmqtt_store_value_t *value,
    lmqtt_encode_buffer_t *encode_buffer)
{
    return encode_buffer_encode_packet_id(encode_buffer,
        LMQTT_TYPE_PUBCOMP << 4, LMQTT_PACKET_ID_SIZE, value->packet_id);
}

LMQTT_STATIC lmqtt_encode_result_t pubcomp_encode_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value, &pubcomp_build, offset,
        buf, buf_len, bytes_written);
}

/******************************************************************************
 * (pingreq) PUBLIC functions
 ******************************************************************************/

LMQTT_STATIC lmqtt_encode_result_t pingreq_build(lmqtt_store_value_t *value,
    lmqtt_encode_buffer_t *encode_buffer)
{
    assert(sizeof(encode_buffer->buf) >= 2);

    encode_buffer->buf[0] = LMQTT_TYPE_PINGREQ << 4;
    encode_buffer->buf[1] = 0;
    encode_buffer->buf_len = 2;
    return LMQTT_ENCODE_FINISHED;
}

LMQTT_STATIC lmqtt_encode_result_t pingreq_encode_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value, pingreq_build, offset,
        buf, buf_len, bytes_written);
}

/******************************************************************************
 * (disconnect) PUBLIC functions
 ******************************************************************************/

LMQTT_STATIC lmqtt_encode_result_t disconnect_build(lmqtt_store_value_t *value,
    lmqtt_encode_buffer_t *encode_buffer)
{
    assert(sizeof(encode_buffer->buf) >= 2);

    encode_buffer->buf[0] = LMQTT_TYPE_DISCONNECT << 4;
    encode_buffer->buf[1] = 0;
    encode_buffer->buf_len = 2;
    return LMQTT_ENCODE_FINISHED;
}

LMQTT_STATIC lmqtt_encode_result_t disconnect_encode_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    size_t offset, unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    return encode_buffer_encode(encode_buffer, value, disconnect_build, offset,
        buf, buf_len, bytes_written);
}

/******************************************************************************
 * lmqtt_tx_buffer_t PRIVATE functions
 ******************************************************************************/

LMQTT_STATIC lmqtt_encoder_t tx_buffer_finder_connect(
    lmqtt_tx_buffer_t *tx_buffer, lmqtt_store_value_t *value)
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

LMQTT_STATIC lmqtt_encoder_t tx_buffer_finder_subscribe(
    lmqtt_tx_buffer_t *tx_buffer, lmqtt_store_value_t *value)
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

LMQTT_STATIC lmqtt_encoder_t tx_buffer_finder_unsubscribe(
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

LMQTT_STATIC lmqtt_encoder_t tx_buffer_finder_publish(
    lmqtt_tx_buffer_t *tx_buffer, lmqtt_store_value_t *value)
{
    lmqtt_publish_t *publish = value->value;

    if (publish->qos == 0) {
        switch (tx_buffer->internal.pos) {
            case 0: return &publish_encode_fixed_header;
            case 1: return &publish_encode_topic;
            case 2: return &publish_encode_payload;
        }
    } else {
        switch (tx_buffer->internal.pos) {
            case 0: return &publish_encode_fixed_header;
            case 1: return &publish_encode_topic;
            case 2: return &publish_encode_packet_id;
            case 3: return &publish_encode_payload;
        }
    }

    publish->internal.encode_count++;
    return 0;
}

LMQTT_STATIC lmqtt_encoder_t tx_buffer_finder_puback(
    lmqtt_tx_buffer_t *tx_buffer, lmqtt_store_value_t *value)
{
    return tx_buffer->internal.pos == 0 ? &puback_encode_fixed_header : 0;
}

LMQTT_STATIC lmqtt_encoder_t tx_buffer_finder_pubrec(
    lmqtt_tx_buffer_t *tx_buffer, lmqtt_store_value_t *value)
{
    return tx_buffer->internal.pos == 0 ? &pubrec_encode_fixed_header : 0;
}

LMQTT_STATIC lmqtt_encoder_t tx_buffer_finder_pubrel(
    lmqtt_tx_buffer_t *tx_buffer, lmqtt_store_value_t *value)
{
    return tx_buffer->internal.pos == 0 ? &pubrel_encode_fixed_header : 0;
}

LMQTT_STATIC lmqtt_encoder_t tx_buffer_finder_pubcomp(
    lmqtt_tx_buffer_t *tx_buffer, lmqtt_store_value_t *value)
{
    return tx_buffer->internal.pos == 0 ? &pubcomp_encode_fixed_header : 0;
}

LMQTT_STATIC lmqtt_encoder_t tx_buffer_finder_pingreq(
    lmqtt_tx_buffer_t *tx_buffer, lmqtt_store_value_t *value)
{
    return tx_buffer->internal.pos == 0 ? &pingreq_encode_fixed_header : 0;
}

LMQTT_STATIC lmqtt_encoder_t tx_buffer_finder_disconnect(
    lmqtt_tx_buffer_t *tx_buffer, lmqtt_store_value_t *value)
{
    return tx_buffer->internal.pos == 0 ? &disconnect_encode_fixed_header : 0;
}

static lmqtt_encoder_finder_t tx_buffer_finder_by_kind_impl(
    lmqtt_kind_t kind)
{
    switch (kind) {
        case LMQTT_KIND_CONNECT: return &tx_buffer_finder_connect;
        case LMQTT_KIND_SUBSCRIBE: return &tx_buffer_finder_subscribe;
        case LMQTT_KIND_UNSUBSCRIBE: return &tx_buffer_finder_unsubscribe;
        case LMQTT_KIND_PUBLISH_0:
        case LMQTT_KIND_PUBLISH_1:
        case LMQTT_KIND_PUBLISH_2: return &tx_buffer_finder_publish;
        case LMQTT_KIND_PUBACK: return &tx_buffer_finder_puback;
        case LMQTT_KIND_PUBREC: return &tx_buffer_finder_pubrec;
        case LMQTT_KIND_PUBREL: return &tx_buffer_finder_pubrel;
        case LMQTT_KIND_PUBCOMP: return &tx_buffer_finder_pubcomp;
        case LMQTT_KIND_PINGREQ: return &tx_buffer_finder_pingreq;
        case LMQTT_KIND_DISCONNECT: return &tx_buffer_finder_disconnect;
    }
    return NULL;
}

/* Enable mocking of tx_buffer_finder_by_kind() */
LMQTT_STATIC lmqtt_encoder_finder_t (*tx_buffer_finder_by_kind)(
    lmqtt_kind_t) = &tx_buffer_finder_by_kind_impl;

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

static lmqtt_io_result_t lmqtt_tx_buffer_encode_impl(lmqtt_tx_buffer_t *state,
    unsigned char *buf, size_t buf_len, size_t *bytes_written)
{
    size_t offset = 0;
    int kind;
    lmqtt_store_value_t value;
    *bytes_written = 0;

    while (!state->closed && lmqtt_store_peek(state->store, &kind, &value)) {
        lmqtt_encoder_finder_t finder = tx_buffer_finder_by_kind(kind);

        if (!finder)
            return LMQTT_IO_ERROR;

        while (1) {
            int result;
            size_t cur_bytes;
            lmqtt_encoder_t encoder = finder(state, &value);

            if (!encoder) {
                if (!kind_expects_response(kind)) {
                    lmqtt_store_drop_current(state->store);

                    if (kind == LMQTT_KIND_DISCONNECT) {
                        lmqtt_tx_buffer_finish(state);
                        break;
                    } else if (value.callback) {
                        value.callback(value.callback_data, value.value);
                    }
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
                return LMQTT_IO_WOULD_BLOCK;
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
        LMQTT_IO_SUCCESS : LMQTT_IO_WOULD_BLOCK;
}

/* Enable mocking of lmqtt_tx_buffer_encode() */
lmqtt_io_result_t (*lmqtt_tx_buffer_encode)(lmqtt_tx_buffer_t *,
    unsigned char *, size_t, size_t *) = &lmqtt_tx_buffer_encode_impl;

lmqtt_string_t *lmqtt_tx_buffer_get_blocking_str(lmqtt_tx_buffer_t *state)
{
    return state->internal.buffer.blocking_str;
}

/******************************************************************************
 * lmqtt_rx_buffer_t PRIVATE functions
 ******************************************************************************/

LMQTT_STATIC int rx_buffer_allocate_put(lmqtt_rx_buffer_t *state, long when,
    lmqtt_message_on_publish_allocate_t allocate, lmqtt_string_t *str,
    size_t len, lmqtt_decode_bytes_t *bytes)
{
    const long rem_pos = state->internal.remain_buf_pos + 1;
    lmqtt_publish_t *publish = &state->internal.publish;
    lmqtt_message_callbacks_t *message = state->message_callbacks;
    /* We may receive a buffer longer than what should be written with
       string_put(), in the case of a topic followed by the packet id and
       payload, or a payload followed by data from other packets; therefore the
       actual value should be capped before continuing */
    size_t max_len = len - (rem_pos - when);
    size_t buf_len = bytes->buf_len > max_len ? max_len : bytes->buf_len;

    if (!state->internal.ignore_publish && rem_pos == when && allocate) {
        switch (allocate(message->on_publish_data, publish, len)) {
            case LMQTT_ALLOCATE_SUCCESS:
                state->internal.ignore_publish = 0;
                break;
            case LMQTT_ALLOCATE_IGNORE:
                state->internal.ignore_publish = 1;
                break;
            default:
                return 0;
        }
    }

    if (!state->internal.ignore_publish) {
        return LMQTT_DECODE_ERROR != string_put(str, bytes->buf, buf_len,
            bytes->bytes_written, &state->internal.blocking_str);
    }
    *bytes->bytes_written += buf_len;
    return 1;
}

LMQTT_STATIC void rx_buffer_deallocate_publish(lmqtt_rx_buffer_t *state)
{
    lmqtt_message_callbacks_t *message = state->message_callbacks;

    if (!state->internal.ignore_publish && message->on_publish_deallocate)
        message->on_publish_deallocate(message->on_publish_data,
            &state->internal.publish);
}

LMQTT_STATIC lmqtt_decode_result_t rx_buffer_decode_connack(
    lmqtt_rx_buffer_t *state, lmqtt_decode_bytes_t *bytes)
{
    unsigned char b;
    lmqtt_connect_t *connect = (lmqtt_connect_t *) state->internal.value.value;

    assert(bytes->buf_len >= 1);
    b = bytes->buf[0];
    *bytes->bytes_written = 0;

    switch (state->internal.remain_buf_pos) {
        case 0:
            if (b & ~1)
                return LMQTT_DECODE_ERROR;
            connect->response.session_present = b;
            *bytes->bytes_written += 1;
            return LMQTT_DECODE_CONTINUE;
        case 1:
            if (b > LMQTT_CONNACK_RC_MAX)
                return LMQTT_DECODE_ERROR;
            connect->response.return_code = b;
            *bytes->bytes_written += 1;
            return LMQTT_DECODE_FINISHED;
        default:
            return LMQTT_DECODE_ERROR;
    }
}

LMQTT_STATIC lmqtt_decode_result_t rx_buffer_decode_publish(
    lmqtt_rx_buffer_t *state, lmqtt_decode_bytes_t *bytes)
{
    size_t *bytes_w;
    long rem_len = state->internal.header.remaining_length;
    long rem_pos = state->internal.remain_buf_pos + 1;
    lmqtt_store_value_t value;
    lmqtt_publish_t *publish = &state->internal.publish;
    lmqtt_message_callbacks_t *message = state->message_callbacks;
    unsigned char qos = state->internal.header.qos;
    static const long s_len = LMQTT_STRING_LEN_SIZE;
    long p_len = qos == 0 ? 0 : LMQTT_PACKET_ID_SIZE;
    lmqtt_packet_id_t packet_id;

    assert(bytes->buf_len >= 1);
    bytes_w = bytes->bytes_written;
    *bytes_w = 0;

    if (rem_pos <= s_len) {
        state->internal.topic_len |= bytes->buf[0] << ((s_len - rem_pos) * 8);
        if (rem_pos == s_len && (state->internal.topic_len == 0 ||
                state->internal.topic_len + s_len + p_len > rem_len))
            return LMQTT_DECODE_ERROR;
        *bytes_w += 1;
    } else {
        long t_len = (long) state->internal.topic_len;
        long p_start = s_len + t_len;

        if (rem_pos == s_len + 1 && (!message->on_publish ||
                !message->on_publish_allocate_topic ||
                !message->on_publish_allocate_payload))
            state->internal.ignore_publish = 1;

        if (rem_pos <= p_start) {
            if (!rx_buffer_allocate_put(state, s_len + 1,
                    message->on_publish_allocate_topic,
                    &state->internal.publish.topic, t_len, bytes)) {
                rx_buffer_deallocate_publish(state);
                return LMQTT_DECODE_ERROR;
            }
        } else if (rem_pos <= p_start + p_len) {
            state->internal.packet_id |= (bytes->buf[0] << ((p_len - rem_pos +
                    p_start) * 8));
            *bytes_w += 1;
        } else {
            if (!rx_buffer_allocate_put(state, p_start + p_len + 1,
                    message->on_publish_allocate_payload,
                    &state->internal.publish.payload,
                    rem_len - p_len - p_start, bytes)) {
                rx_buffer_deallocate_publish(state);
                return LMQTT_DECODE_ERROR;
            }
        }
    }

    if (state->internal.blocking_str)
        return LMQTT_DECODE_WOULD_BLOCK;
    if (rem_len >= rem_pos + *bytes_w)
        return LMQTT_DECODE_CONTINUE;

    packet_id = state->internal.packet_id;

    if (qos > 0) {
        memset(&value, 0, sizeof(value));
        value.packet_id = packet_id;
        lmqtt_store_append(state->store, qos == 2 ? LMQTT_KIND_PUBREC :
            LMQTT_KIND_PUBACK, &value);
    }

    if (qos < 2 || !lmqtt_id_set_contains(&state->id_set, packet_id)) {
        if (qos == 2 && !lmqtt_id_set_put(&state->id_set, packet_id)) {
            rx_buffer_deallocate_publish(state);
            /* Here we return an error despite having already increased the
               bytes_written count, unlike everywhere else. I don't know which
               one is the "correct" behavior; so, unless some issue justifying
               a specific one appears, let's leave it like that, since at this
               point the connection failed and the user should reset the client
               anyway. */
            return LMQTT_DECODE_ERROR;
        }

        publish->qos = qos;
        publish->retain = state->internal.header.retain;

        if (!state->internal.ignore_publish && message->on_publish)
            message->on_publish(message->on_publish_data, publish);
    }

    rx_buffer_deallocate_publish(state);
    return LMQTT_DECODE_FINISHED;
}

LMQTT_STATIC lmqtt_decode_result_t rx_buffer_decode_suback(
    lmqtt_rx_buffer_t *state, lmqtt_decode_bytes_t *bytes)
{
    unsigned char b;
    lmqtt_subscribe_t *subscribe =
        (lmqtt_subscribe_t *) state->internal.value.value;
    long pos = state->internal.remain_buf_pos - LMQTT_PACKET_ID_SIZE;

    assert(bytes->buf_len >= 1);
    b = bytes->buf[0];
    *bytes->bytes_written = 0;

    if (pos == 0) {
        long len = state->internal.header.remaining_length -
            LMQTT_PACKET_ID_SIZE;
        if (len != (long) subscribe->count)
            return LMQTT_DECODE_ERROR;
    }

    if (b > 2 && b != 0x80)
        return LMQTT_DECODE_ERROR;

    subscribe->subscriptions[pos].return_code = b;
    *bytes->bytes_written += 1;
    return pos + 1 >= (long) subscribe->count ?
        LMQTT_DECODE_FINISHED : LMQTT_DECODE_CONTINUE;
}

/*
 * Return: 1 on success, 0 on failure
 */
static int rx_buffer_call_callback_impl(lmqtt_rx_buffer_t *state)
{
    lmqtt_store_value_t *value = &state->internal.value;

    if (value->callback)
        return value->callback(value->callback_data, value->value);

    return 0;
}

/* Enable mocking of rx_buffer_call_callback() */
LMQTT_STATIC int (*rx_buffer_call_callback)(
    lmqtt_rx_buffer_t *) = &rx_buffer_call_callback_impl;

static lmqtt_decode_result_t rx_buffer_decode_type_impl(
    lmqtt_rx_buffer_t *state, lmqtt_decode_bytes_t *bytes)
{
    if (!state->internal.decoder->decode_bytes) {
        *bytes->bytes_written = 0;
        return LMQTT_DECODE_ERROR;
    }

    return state->internal.decoder->decode_bytes(state, bytes);
}

/* Enable mocking of rx_buffer_decode_type() */
LMQTT_STATIC lmqtt_decode_result_t (*rx_buffer_decode_type)(
    lmqtt_rx_buffer_t *, lmqtt_decode_bytes_t *) = &rx_buffer_decode_type_impl;

LMQTT_STATIC lmqtt_io_result_t rx_buffer_fail(lmqtt_rx_buffer_t *state)
{
    state->internal.failed = 1;
    return LMQTT_IO_ERROR;
}

LMQTT_STATIC int rx_buffer_pop_packet(lmqtt_rx_buffer_t *state,
    lmqtt_packet_id_t packet_id)
{
    return lmqtt_store_pop_marked_by(state->store,
        state->internal.decoder->kind, packet_id, &state->internal.value);
}

LMQTT_STATIC int rx_buffer_is_packet_finished(lmqtt_rx_buffer_t *state)
{
    return state->internal.header_finished && state->internal.remain_buf_pos >=
        state->internal.header.remaining_length;
}

LMQTT_STATIC int rx_buffer_finish_packet(lmqtt_rx_buffer_t *state)
{
    int result;

    if (state->internal.decoder->kind == LMQTT_KIND_PUBLISH_2)
        result = lmqtt_store_append(state->store, LMQTT_KIND_PUBREL,
            &state->internal.value);
    else
        result = rx_buffer_call_callback(state);

    lmqtt_rx_buffer_reset(state);

    return result;
}

LMQTT_STATIC int rx_buffer_pop_packet_without_id(lmqtt_rx_buffer_t *state)
{
    return rx_buffer_pop_packet(state, 0);
}

LMQTT_STATIC int rx_buffer_pop_packet_with_id(lmqtt_rx_buffer_t *state)
{
    return rx_buffer_pop_packet(state, state->internal.packet_id);
}

LMQTT_STATIC int rx_buffer_pop_packet_ignore(lmqtt_rx_buffer_t *state)
{
    return 1;
}

LMQTT_STATIC int rx_buffer_pubrel(lmqtt_rx_buffer_t *state)
{
    static lmqtt_store_value_t value;
    lmqtt_packet_id_t packet_id = state->internal.packet_id;

    lmqtt_id_set_remove(&state->id_set, packet_id);

    memset(&value, 0, sizeof(value));
    value.packet_id = packet_id;
    return lmqtt_store_append(state->store, LMQTT_KIND_PUBCOMP, &value);
}

LMQTT_STATIC lmqtt_decode_result_t rx_buffer_decode_remaining_without_id(
    lmqtt_rx_buffer_t *state, lmqtt_decode_bytes_t *bytes)
{
    int res = rx_buffer_decode_type(state, bytes);
    long rem_pos = state->internal.remain_buf_pos + *bytes->bytes_written;
    long rem_len = state->internal.header.remaining_length;

    if (res == LMQTT_DECODE_ERROR)
        return LMQTT_DECODE_ERROR;
    if (res != LMQTT_DECODE_FINISHED && rem_pos >= rem_len)
        return LMQTT_DECODE_ERROR;
    if (res == LMQTT_DECODE_FINISHED && rem_pos != rem_len)
        return LMQTT_DECODE_ERROR;

    return res;
}

LMQTT_STATIC lmqtt_decode_result_t rx_buffer_decode_remaining_with_id(
    lmqtt_rx_buffer_t *state, lmqtt_decode_bytes_t *bytes)
{
    long rem_pos = state->internal.remain_buf_pos + 1;
    static const long p_len = LMQTT_PACKET_ID_SIZE;

    if (rem_pos > p_len)
        return rx_buffer_decode_remaining_without_id(state, bytes);

    assert(bytes->buf_len >= 1);
    state->internal.packet_id |= bytes->buf[0] << ((p_len - rem_pos) * 8);
    *bytes->bytes_written = 1;

    if (rem_pos == p_len && !state->internal.decoder->pop_packet_with_id(state))
        return LMQTT_DECODE_ERROR;

    return LMQTT_DECODE_CONTINUE;
}

static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_connack = {
    2,
    LMQTT_KIND_CONNECT,
    &rx_buffer_pop_packet_without_id,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_decode_remaining_without_id,
    &rx_buffer_decode_connack
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_publish = {
    3, /* this should be 3 or 5 depending on QoS, but rx_buffer_decode_publish()
          will also validate it */
    0, /* never used */
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_decode_remaining_without_id,
    &rx_buffer_decode_publish
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_puback = {
    2,
    LMQTT_KIND_PUBLISH_1,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pop_packet_with_id,
    &rx_buffer_decode_remaining_with_id,
    NULL
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_pubrec = {
    2,
    LMQTT_KIND_PUBLISH_2,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pop_packet_with_id,
    &rx_buffer_decode_remaining_with_id,
    NULL
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_pubrel = {
    2,
    0, /* never used */
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pubrel,
    &rx_buffer_decode_remaining_with_id,
    NULL
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_pubcomp = {
    2,
    LMQTT_KIND_PUBREL,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pop_packet_with_id,
    &rx_buffer_decode_remaining_with_id,
    NULL
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_suback = {
    3,
    LMQTT_KIND_SUBSCRIBE,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pop_packet_with_id,
    &rx_buffer_decode_remaining_with_id,
    &rx_buffer_decode_suback
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_unsuback = {
    2,
    LMQTT_KIND_UNSUBSCRIBE,
    &rx_buffer_pop_packet_ignore,
    &rx_buffer_pop_packet_with_id,
    &rx_buffer_decode_remaining_with_id,
    NULL
};
static const struct _lmqtt_rx_buffer_decoder_t rx_buffer_decoder_pingresp = {
    0,
    LMQTT_KIND_PINGREQ,
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
    rx_buffer_call_callback(state);
}

static lmqtt_io_result_t lmqtt_rx_buffer_decode_impl(lmqtt_rx_buffer_t *state,
    unsigned char *buf, size_t buf_len, size_t *bytes_read)
{
    int i = 0;
    *bytes_read = 0;

    if (state->internal.failed)
        return LMQTT_IO_ERROR;

    while (i < buf_len) {
        if (!state->internal.header_finished) {
            long rem_len;
            int res = fixed_header_decode(&state->internal.header, buf[i]);

            if (res == LMQTT_DECODE_ERROR)
                return rx_buffer_fail(state);

            i += 1;
            *bytes_read += 1;
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
            lmqtt_decode_result_t res;
            lmqtt_decode_bytes_t bytes;
            size_t cnt = 0;
            bytes.buf_len = buf_len - i;
            bytes.buf = &buf[i];
            bytes.bytes_written = &cnt;

            res = state->internal.decoder->decode_remaining(state, &bytes);
            if (res == LMQTT_DECODE_FINISHED || res == LMQTT_DECODE_CONTINUE) {
                i += cnt;
                *bytes_read += cnt;
                state->internal.remain_buf_pos += cnt;
            } else if (res == LMQTT_DECODE_WOULD_BLOCK) {
                assert(*bytes.bytes_written == 0);
                break;
            } else {
                return rx_buffer_fail(state);
            }
        }

        if (rx_buffer_is_packet_finished(state))
            rx_buffer_finish_packet(state);
    }

    if (*bytes_read > 0) {
        /* If decode_remaining() returns WOULD_BLOCK after we have successfully
           decoded other bytes we should not signal that some string is
           blocking, and instead wait until the decoder is called again */
        state->internal.blocking_str = NULL;
        return LMQTT_IO_SUCCESS;
    } else if (buf_len == 0) {
        return LMQTT_IO_SUCCESS;
    } else {
        return LMQTT_IO_WOULD_BLOCK;
    }
}

/* Enable mocking of lmqtt_rx_buffer_decode() */
lmqtt_io_result_t (*lmqtt_rx_buffer_decode)(lmqtt_rx_buffer_t *,
    unsigned char *, size_t, size_t *) = &lmqtt_rx_buffer_decode_impl;

lmqtt_string_t *lmqtt_rx_buffer_get_blocking_str(lmqtt_rx_buffer_t *state)
{
    return state->internal.blocking_str;
}
