#ifndef _LIGHTMQTT_CORE_H_
#define _LIGHTMQTT_CORE_H_

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <stddef.h>

typedef enum {
    LMQTT_IO_SUCCESS = 0,
    LMQTT_IO_WOULD_BLOCK,
    LMQTT_IO_ERROR
} lmqtt_io_result_t;

typedef lmqtt_io_result_t (*lmqtt_read_t)(void *, void *, size_t, size_t *);
typedef lmqtt_io_result_t (*lmqtt_write_t)(void *, void *, size_t, size_t *);

#ifdef LMQTT_TEST
    #define LMQTT_STATIC
#else
    #define LMQTT_STATIC static
#endif

#endif
