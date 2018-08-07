AM_CPPFLAGS = -I$(top_srcdir)/include/avcap -I$(top_srcdir)/include/avcap/windows $(AVCAP_DEPS_CFLAGS) -DAVCAP_EXPORTS

noinst_LTLIBRARIES = libwindowsavcap.la
libwindowsavcap_la_SOURCES = \
	DS_Connector.cpp              DS_Device.cpp\
	DS_ConnectorManager.cpp\
	DS_Control.cpp          	  DS_DeviceDescriptor.cpp\
	DS_ControlManager.cpp		  Crossbar.cpp\
	DS_FormatManager.cpp		  DS_VidCapManager.cpp\
	DS_Tuner.cpp				  SampleGrabberCallback.cpp


