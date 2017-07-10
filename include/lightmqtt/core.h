#ifndef _LIGHTMQTT_CORE_H_
#define _LIGHTMQTT_CORE_H_

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <stddef.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum {
    LMQTT_IO_SUCCESS = 0,
    LMQTT_IO_WOULD_BLOCK,
    LMQTT_IO_ERROR
} lmqtt_io_result_t;

typedef enum {
    /* bug in encoding function */
    LMQTT_ERROR_ENCODE_INTERNAL = 1,
    /* OS error reading string to build outgoing packet */
    LMQTT_ERROR_ENCODE_STRING,
    /* invalid upper nibble in fixed header of incoming packet */
    LMQTT_ERROR_DECODE_FIXED_HEADER_INVALID_TYPE,
    /* invalid lower nibble in fixed header of incoming packet */
    LMQTT_ERROR_DECODE_FIXED_HEADER_INVALID_FLAGS,
    /* invalid remaining length value */
    LMQTT_ERROR_DECODE_FIXED_HEADER_INVALID_REMAINING_LENGTH,
    LMQTT_ERROR_DECODE_CONNACK_INVALID_ACKNOWLEDGE_FLAGS,
    LMQTT_ERROR_DECODE_CONNACK_INVALID_RETURN_CODE,
    LMQTT_ERROR_DECODE_CONNACK_INVALID_LENGTH,
    LMQTT_ERROR_DECODE_PUBLISH_INVALID_LENGTH,
    LMQTT_ERROR_DECODE_PUBLISH_ID_SET_FULL,
    LMQTT_ERROR_DECODE_PUBLISH_ALLOCATE_TOPIC_FAILED,
    LMQTT_ERROR_DECODE_PUBLISH_ALLOCATE_PAYLOAD_FAILED,
    LMQTT_ERROR_DECODE_PUBLISH_WRITE_TOPIC_FAILED,
    LMQTT_ERROR_DECODE_PUBLISH_WRITE_PAYLOAD_FAILED
} lmqtt_error_t;

typedef lmqtt_io_result_t (*lmqtt_read_t)(void *, void *, size_t, size_t *,
    int *);
typedef lmqtt_io_result_t (*lmqtt_write_t)(void *, void *, size_t, size_t *,
    int *);

#ifdef  __cplusplus
}
#endif

#ifdef LMQTT_TEST
    #define LMQTT_STATIC
#else
    #define LMQTT_STATIC static
#endif

#endif
