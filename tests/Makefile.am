TESTS = .bin/check_lightmqtt
check_PROGRAMS = .bin/check_lightmqtt
_bin_check_lightmqtt_SOURCES = check_lightmqtt.c \
    check_encode_remaining_length.c \
    check_validate_connect.c \
    check_encode_connect.c
_bin_check_lightmqtt_CFLAGS = -I$(top_builddir) @CHECK_CFLAGS@
_bin_check_lightmqtt_LDADD = $(top_builddir)/src/liblightmqtt.la @CHECK_LIBS@
