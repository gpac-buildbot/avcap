AM_CPPFLAGS = -I$(top_srcdir)/include $(AVCAP_DEPS_CFLAGS)

bin_PROGRAMS = captest
captest_SOURCES = captest.cpp TestCaptureHandler.cpp TestCaptureHandler.h

captest_LDADD = $(top_builddir)/avcap/libavcap.la
captest_DEPENDENCIES = $(top_builddir)/avcap/libavcap.la

EXTRA_DIST = windows/getopt.h windows/getopt_long.c windows/getopt.c windows/COPYING
