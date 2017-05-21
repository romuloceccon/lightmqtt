bin_PROGRAMS = reconnect pingpong

reconnect_SOURCES = reconnect.c helpers.c
pingpong_SOURCES = pingpong.c helpers.c

AM_CFLAGS = -I$(top_srcdir) -std=gnu99
LDADD = $(top_builddir)/src/liblightmqtt.la
