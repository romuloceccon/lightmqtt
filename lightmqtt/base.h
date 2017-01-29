#ifndef _LIGHTMQTT_BASE_H_
#define _LIGHTMQTT_BASE_H_

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#ifdef HAVE_STDINT_H
    #include <stdint.h>
    typedef uint8_t u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
#else
    typedef unsigned char u8;
    typedef unsigned short u16;
    typedef unsigned long u32;
#endif

#endif
