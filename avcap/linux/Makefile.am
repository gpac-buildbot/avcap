AM_CPPFLAGS = -I$(top_srcdir)/include/avcap -I$(top_srcdir)/include/avcap/linux $(AVCAP_DEPS_CFLAGS)

noinst_LTLIBRARIES = liblinuxavcap.la
liblinuxavcap_la_SOURCES = \
	raw1394util.cpp           V4L2_ConnectorManager.cpp\
	AVC_Device.cpp            V4L2_ControlBase.cpp\
	AVC_DeviceDescriptor.cpp  V4L1_ConnectorManager.cpp  V4L2_ControlManager.cpp\
	AVC_FormatManager.cpp     V4L1_Control.cpp           V4L2_Device.cpp\
	AVC_Reader.cpp            V4L1_ControlManager.cpp    V4L2_DeviceDescriptor.cpp\
	AVC_VidCapManager.cpp     V4L1_Device.cpp\
	error.cpp                 V4L1_DeviceDescriptor.cpp  V4L2_FormatManager.cpp\
	frame.cpp                 V4L1_FormatManager.cpp     V4L2_MenuControl.cpp\
	ieee1394io.cpp            V4L1_VidCapManager.cpp     V4L2_Tuner.cpp\
	V4L2_Connector.cpp        V4L2_VidCapManager.cpp	
