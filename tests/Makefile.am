TESTS = \
    .bin/check_encode_remaining_length \
    .bin/check_connect_encode_headers \
    .bin/check_connect_encode_payload \
    .bin/check_connect_validate \
    .bin/check_fixed_header_decode \
    .bin/check_connack_decode \
    .bin/check_process \
    .bin/check_tx_buffer_encode \
    .bin/check_tx_buffer_connect \
    .bin/check_rx_buffer_decode


check_PROGRAMS = $(TESTS)

_bin_check_encode_remaining_length_SOURCES  = check_encode_remaining_length.c
_bin_check_connect_encode_headers_SOURCES   = check_connect_encode_headers.c
_bin_check_connect_encode_payload_SOURCES   = check_connect_encode_payload.c
_bin_check_connect_validate_SOURCES         = check_connect_validate.c
_bin_check_fixed_header_decode_SOURCES      = check_fixed_header_decode.c
_bin_check_connack_decode_SOURCES           = check_connack_decode.c
_bin_check_process_SOURCES                  = check_process.c
_bin_check_tx_buffer_encode_SOURCES         = check_tx_buffer_encode.c
_bin_check_tx_buffer_connect_SOURCES        = check_tx_buffer_connect.c
_bin_check_rx_buffer_decode_SOURCES         = check_rx_buffer_decode.c

AM_CFLAGS = -I$(top_builddir) @CHECK_CFLAGS@
LDADD = $(top_builddir)/src/liblightmqtt.la @CHECK_LIBS@
