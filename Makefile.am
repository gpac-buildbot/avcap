SUBDIRS = avcap include test
DISTCLEANFILES = avcap-config.h _configs.sed
pkgconfigdir = @libdir@/pkgconfig

avcap.pc: ${top_builddir}/config.status ${top_srcdir}/avcap.pc.in
	${top_builddir}/config.status --file $@:${top_srcdir}/avcap.pc.in

EXTRA_DIST = avcap.pc.in\
	contrib/avcap.sln contrib/avcap.vcproj contrib/captest.vcproj doc \
	bin/win32/captest.exe  bin/win32/libavcap-6.dll  bin/win32/libavcap.a  bin/win32/libavcap.dll.a

pkgconfig_DATA = avcap.pc

CLEANFILES = avcap.pc

install-exec-hook:
	if test "`uname -s`" == "Linux"; then \
		ldconfig; \
	fi
