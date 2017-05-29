#ifndef _LIGHTMQTT_CORE_H_
#define _LIGHTMQTT_CORE_H_

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

typedef enum {
    LMQTT_IO_SUCCESS = 0,
    LMQTT_IO_AGAIN,
    LMQTT_IO_ERROR
} lmqtt_io_result_t;

#ifdef LMQTT_TEST
    #define LMQTT_STATIC
#else
    #define LMQTT_STATIC static
#endif

#endif
