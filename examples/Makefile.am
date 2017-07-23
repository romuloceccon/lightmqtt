noinst_PROGRAMS = reconnect pingpong

reconnect_SOURCES = reconnect.c helpers.c
pingpong_SOURCES = pingpong.c helpers.c

AM_CFLAGS = -I$(top_srcdir)/include -I$(srcdir) -D_GNU_SOURCE -std=gnu99
LDADD = $(top_builddir)/src/liblightmqtt.la
