bin_PROGRAMS = connect pingpong

connect_SOURCES = connect.c
pingpong_SOURCES = pingpong.c helpers.c

AM_CFLAGS = -I$(top_srcdir) -std=gnu99
LDADD = $(top_builddir)/src/liblightmqtt.la
