TESTS = \
    .bin/check_time \
    .bin/check_store \
    .bin/check_encode_remaining_length \
    .bin/check_encode_buffer \
    .bin/check_connect_encode_headers \
    .bin/check_connect_encode_payload \
    .bin/check_connect_validate \
    .bin/check_subscribe_validate \
    .bin/check_fixed_header_decode \
    .bin/check_tx_buffer_encode \
    .bin/check_tx_buffer_finders \
    .bin/check_rx_buffer_decode \
    .bin/check_rx_buffer_decode_connack \
    .bin/check_rx_buffer_decode_suback \
    .bin/check_rx_buffer_callbacks \
    .bin/check_rx_buffer_finish \
    .bin/check_client_buffers \
    .bin/check_client_commands

check_PROGRAMS = $(TESTS)

_bin_check_time_SOURCES                     = check_time.c \
    check_lightmqtt.c
_bin_check_store_SOURCES                    = check_store.c \
    check_lightmqtt.c
_bin_check_encode_remaining_length_SOURCES  = check_encode_remaining_length.c
_bin_check_encode_buffer_SOURCES            = check_encode_buffer.c
_bin_check_connect_encode_headers_SOURCES   = check_connect_encode_headers.c
_bin_check_connect_encode_payload_SOURCES   = check_connect_encode_payload.c
_bin_check_connect_validate_SOURCES         = check_connect_validate.c
_bin_check_subscribe_validate_SOURCES       = check_subscribe_validate.c
_bin_check_fixed_header_decode_SOURCES      = check_fixed_header_decode.c
_bin_check_tx_buffer_encode_SOURCES         = check_tx_buffer_encode.c \
    check_lightmqtt.c
_bin_check_tx_buffer_finders_SOURCES        = check_tx_buffer_finders.c \
    check_lightmqtt.c
_bin_check_rx_buffer_decode_SOURCES         = check_rx_buffer_decode.c \
    check_lightmqtt.c
_bin_check_rx_buffer_decode_connack_SOURCES = check_rx_buffer_decode_connack.c
_bin_check_rx_buffer_decode_suback_SOURCES  = check_rx_buffer_decode_suback.c
_bin_check_rx_buffer_callbacks_SOURCES      = check_rx_buffer_callbacks.c \
    check_lightmqtt.c
_bin_check_rx_buffer_finish_SOURCES         = check_rx_buffer_finish.c \
    check_lightmqtt.c
_bin_check_client_buffers_SOURCES           = check_client_buffers.c \
    check_lightmqtt.c
_bin_check_client_commands_SOURCES          = check_client_commands.c \
    check_lightmqtt.c

AM_CFLAGS = -I$(top_builddir) @CHECK_CFLAGS@
LDADD = $(top_builddir)/src/liblightmqtt.la @CHECK_LIBS@
