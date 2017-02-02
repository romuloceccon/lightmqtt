TESTS = .bin/check_lightmqtt
check_PROGRAMS = .bin/check_lightmqtt
_bin_check_lightmqtt_SOURCES = check_lightmqtt.c \
    check_encode_remaining_length.c \
    check_validate_connect.c \
    check_encode_connect_headers.c \
    check_encode_connect_payload.c \
    check_build_tx_buffer.c \
    check_decode_connack.c \
    check_decode_fixed_header.c
_bin_check_lightmqtt_CFLAGS = -I$(top_builddir) @CHECK_CFLAGS@
_bin_check_lightmqtt_LDADD = $(top_builddir)/src/liblightmqtt.la @CHECK_LIBS@
