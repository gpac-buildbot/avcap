#!/bin/sh -e

if [ $# -ne 1 ] ; then
  echo "Usage: `basename $0` platform"
  exit 1
fi


if [ ! -e /usr/include/linux/videodev.h ] ; then
  if [ ! -e /usr/include/linux/videodev2.h ] ; then
    echo "This platform does not have Video4Linux(2) support. Abort"
    exit 0
  fi
fi

chmod +x configure
if [ -n "`uname -a | grep x86_64`" ] ; then
  ./configure --with-pic
else
  ./configure
fi


make || exit 1
mkdir -p ../../gpac_public/extra_lib/lib/gcc
cp avcap/.libs/libavcap.a ../../gpac_public/extra_lib/lib/gcc

if test $1 = "osx" ; then
mkdir ../../gpac_public/extra_lib/include/avcap/osx
cp avcap-config.h ../../gpac_public/extra_lib/include/avcap/osx
fi

if test $1 = "linux" ; then
mkdir -p ../../gpac_public/extra_lib/include/avcap/linux
cp avcap-config.h ../../gpac_public/extra_lib/include/avcap/linux
fi
