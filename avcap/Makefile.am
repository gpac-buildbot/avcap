SUBDIRS = $(PLATFORM_SUBDIR)
DIST_SUBDIRS = linux osx windows

AM_CPPFLAGS = -I$(top_srcdir)/include/avcap -I$(top_srcdir)/include/avcap/$(PLATFORM_SUBDIR) $(AVCAP_DEPS_CFLAGS) -DAVCAP_EXPORTS

lib_LTLIBRARIES = libavcap.la
libavcap_la_SOURCES = \
	FormatManager.cpp\
	ConnectorManager.cpp  		DeviceCollector.cpp        IOBuffer.cpp\
	ControlManager.cpp    		DeviceDescriptor.cpp

libavcap_la_LIBADD = $(PLATFORM_LIB)
libavcap_la_LDFLAGS = \
	$(AVCAP_DEPS_LIBS) \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
# 	-release $(LT_RELEASE)

libavcap_la_DEPENDENCIES = $(PLATFORM_LIB)
