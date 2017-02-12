TESTS = \
    .bin/check_encode_remaining_length \
    .bin/check_encode_connect_headers \
    .bin/check_encode_connect_payload \
    .bin/check_validate_connect \
    .bin/check_fixed_header_decode \
    .bin/check_connack_decode \
    .bin/check_process_input \
    .bin/check_process_output \
    .bin/check_encode_tx_buffer \
    .bin/check_decode_rx_buffer


check_PROGRAMS = $(TESTS)

_bin_check_encode_remaining_length_SOURCES  = check_encode_remaining_length.c
_bin_check_encode_connect_headers_SOURCES   = check_encode_connect_headers.c
_bin_check_encode_connect_payload_SOURCES   = check_encode_connect_payload.c
_bin_check_validate_connect_SOURCES         = check_validate_connect.c
_bin_check_fixed_header_decode_SOURCES      = check_fixed_header_decode.c
_bin_check_connack_decode_SOURCES           = check_connack_decode.c
_bin_check_process_input_SOURCES            = check_process_input.c
_bin_check_process_output_SOURCES           = check_process_output.c
_bin_check_encode_tx_buffer_SOURCES         = check_encode_tx_buffer.c
_bin_check_decode_rx_buffer_SOURCES         = check_decode_rx_buffer.c

AM_CFLAGS = -I$(top_builddir) @CHECK_CFLAGS@
LDADD = $(top_builddir)/src/liblightmqtt.la @CHECK_LIBS@
