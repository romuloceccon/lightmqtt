check_PROGRAMS = check_time check_store check_id_set check_string \
    check_encode_remaining_length check_encode_buffer \
    check_connect_encode_headers check_connect_encode_payload \
    check_connect_validate check_subscribe_validate check_publish_encode \
    check_publish_validate check_fixed_header_decode check_tx_buffer_encode \
    check_tx_buffer_finders check_rx_buffer_decode \
    check_rx_buffer_decode_connack check_rx_buffer_decode_publish \
    check_rx_buffer_decode_pubrel check_rx_buffer_decode_suback \
    check_rx_buffer_callbacks check_client_buffers check_client_commands \
    check_client_run_once

TESTS = $(check_PROGRAMS)

TEST_BASE_SRCS = check_lightmqtt.c test_store.c test_time.c
TEST_PACKET_SRCS = test_packet.c $(TEST_BASE_SRCS)
TEST_IO_SRCS = test_io.c test_packet.c $(TEST_BASE_SRCS)

check_time_SOURCES                    = check_time.c $(TEST_PACKET_SRCS)
check_store_SOURCES                   = check_store.c $(TEST_PACKET_SRCS)
check_id_set_SOURCES                  = check_id_set.c $(TEST_PACKET_SRCS)
check_string_SOURCES                  = check_string.c $(TEST_PACKET_SRCS)
check_encode_remaining_length_SOURCES = check_encode_remaining_length.c $(TEST_PACKET_SRCS)
check_encode_buffer_SOURCES           = check_encode_buffer.c $(TEST_PACKET_SRCS)
check_connect_encode_headers_SOURCES  = check_connect_encode_headers.c $(TEST_PACKET_SRCS)
check_connect_encode_payload_SOURCES  = check_connect_encode_payload.c $(TEST_PACKET_SRCS)
check_connect_validate_SOURCES        = check_connect_validate.c $(TEST_PACKET_SRCS)
check_subscribe_validate_SOURCES      = check_subscribe_validate.c $(TEST_PACKET_SRCS)
check_publish_encode_SOURCES          = check_publish_encode.c $(TEST_PACKET_SRCS)
check_publish_validate_SOURCES        = check_publish_validate.c $(TEST_PACKET_SRCS)
check_fixed_header_decode_SOURCES     = check_fixed_header_decode.c $(TEST_PACKET_SRCS)
check_tx_buffer_encode_SOURCES        = check_tx_buffer_encode.c $(TEST_PACKET_SRCS)
check_tx_buffer_finders_SOURCES       = check_tx_buffer_finders.c $(TEST_PACKET_SRCS)
check_rx_buffer_decode_SOURCES        = check_rx_buffer_decode.c $(TEST_PACKET_SRCS)
check_rx_buffer_decode_connack_SOURCES = check_rx_buffer_decode_connack.c $(TEST_PACKET_SRCS)
check_rx_buffer_decode_publish_SOURCES = check_rx_buffer_decode_publish.c $(TEST_PACKET_SRCS)
check_rx_buffer_decode_pubrel_SOURCES = check_rx_buffer_decode_pubrel.c $(TEST_PACKET_SRCS)
check_rx_buffer_decode_suback_SOURCES = check_rx_buffer_decode_suback.c $(TEST_PACKET_SRCS)
check_rx_buffer_callbacks_SOURCES     = check_rx_buffer_callbacks.c $(TEST_PACKET_SRCS)
check_client_buffers_SOURCES          = check_client_buffers.c $(TEST_IO_SRCS)
check_client_commands_SOURCES         = check_client_commands.c $(TEST_IO_SRCS)
check_client_run_once_SOURCES         = check_client_run_once.c $(TEST_IO_SRCS)

AM_CFLAGS = -I$(top_builddir) @CHECK_CFLAGS@
LDADD = @CHECK_LIBS@
