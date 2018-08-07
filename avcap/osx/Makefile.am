AM_CPPFLAGS = -I$(top_srcdir)/include/avcap -I$(top_srcdir)/include/avcap/osx $(AVCAP_DEPS_CFLAGS)

noinst_LTLIBRARIES = libosxavcap.la
libosxavcap_la_SOURCES =\
	QT_ConnectorManager.cpp QT_ControlManager.cpp  QT_Device.cpp\
	QT_DeviceDescriptor.cpp QT_FormatManager.cpp   QT_VidCapManager.cpp\
	QT_DeviceEnumerator.cpp QT_Control.cpp

# libosxavcap_la_LDFLAGS = -framework QuickTime
