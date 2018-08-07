/*
 * (c) 2005, 2008 Nico Pranke <Nico.Pranke@googlemail.com>, Robin Luedtke <RobinLu@gmx.de>
 *
 * This file is part of avcap.
 *
 * avcap is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * avcap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with avcap.  If not, see <http://www.gnu.org/licenses/>.
 */

/* avcap is free for non-commercial use.
 * To use it in commercial endeavors, please contact Nico Pranke <Nico.Pranke@googlemail.com>.
 */

#include <string.h>
#include <sstream>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/types.h>
#include <unistd.h>
#include <errno.h>

#include "V4L2_DeviceDescriptor.h"
#include "V4L2_Device.h"
#include "log.h"

#ifdef AVCAP_HAVE_V4L2
#include <linux/videodev2.h>
#else
#include <linux/videodev.h>
#endif

using namespace avcap;

// Construction & Destruction

V4L2_DeviceDescriptor::V4L2_DeviceDescriptor(const std::string &name):
	mName(name), mCapabilities(0), mHandle(-1), mDevice(0)
{
	// test whether it is a V4L2 device and query its capabilities
	mValid = queryCapabilities();
}

V4L2_DeviceDescriptor::~V4L2_DeviceDescriptor()
{
	close();
}

CaptureDevice* V4L2_DeviceDescriptor::getDevice()
{
	return mDevice;
}

int V4L2_DeviceDescriptor::open()
{
	// check if device-file is already open,
	// so multiple calls to open() will result in an error
	if(mHandle != -1)
		return -1;

	// Open the device file handle
	mHandle = ::open((const char*) mName.c_str(), O_RDWR /*| O_NONBLOCK*/ );

	if (mHandle < 0)
		return -1;

	if(mDevice == 0)
		mDevice = new V4L2_Device(this);

	return 0;
}

int V4L2_DeviceDescriptor::close()
{
	int res = 0;

	//  close the device file handle
	if(mHandle != -1)
		res = ::close(mHandle);

	mHandle = -1;

	delete mDevice;
	mDevice = 0;

	return  0;
}

bool V4L2_DeviceDescriptor::queryCapabilities()
{
	// test and query device capabilities. return true, if V4L2 device and false, if not.

	// temporary open
	int file = ::open((const char*) mName.c_str(), O_RDWR /*| O_NONBLOCK*/ );
	// failed
	if ( file < 0 )
		return false;

	struct v4l2_capability caps;
	memset(&caps, 0, sizeof (v4l2_capability));

	// query the capabilities
	if (ioctl(file, VIDIOC_QUERYCAP, &caps) < 0) {
		// its obviously no v4l2 device
		::close(file);
		return false;
	}

	// store the driver and device infos
	mDriver = (char*) caps.driver;
	mCard 	= (char*) caps.card;
	mInfo	= (char*) caps.bus_info;
	mVersion= caps.version;
	mCapabilities = caps.capabilities;

	std::ostringstream res;
	res<<((mVersion >> 16) & 0xFF)<<"."<<((mVersion >> 8) & 0xFF)<<"."<<(mVersion & 0xFF);
	mVersionString = res.str();

	logDebug("V4L2_DeviceDescriptor::queryCapabilities(): Device found: Driver: " + mDriver +
			" Card: " + mCard +
			" Bus: " + mInfo +
			" Version: " + getVersionString());

	::close (file);

	return true;
}

const std::string& V4L2_DeviceDescriptor::getVersionString() const
{
	return mVersionString;
}

const std::string& V4L2_DeviceDescriptor::getName() const
{
	return mName;
}


bool V4L2_DeviceDescriptor::isAVDev() const
{
	// Return true if it is a V4L2 device, false otherwise.
	return mValid;
}

bool V4L2_DeviceDescriptor::isVideoCaptureDev() const
{
	return mCapabilities & V4L2_CAP_VIDEO_CAPTURE;
}

bool V4L2_DeviceDescriptor::isTuner() const
{
	return mCapabilities & V4L2_CAP_TUNER;
}

bool V4L2_DeviceDescriptor::isAudioDev() const
{
	return mCapabilities & V4L2_CAP_AUDIO;
}

bool V4L2_DeviceDescriptor::isOverlayDev() const
{
	return mCapabilities & V4L2_CAP_VIDEO_OVERLAY;
}

bool V4L2_DeviceDescriptor::isRWDev () const
{
	return mCapabilities & V4L2_CAP_READWRITE;
}

bool V4L2_DeviceDescriptor::isAsyncIODev() const
{
	return mCapabilities & V4L2_CAP_ASYNCIO;
}

bool V4L2_DeviceDescriptor::isStreamingDev() const
{
	return mCapabilities & V4L2_CAP_STREAMING;
}

bool V4L2_DeviceDescriptor::isRadioDev() const
{
#ifdef V4L2_CAP_RADIO
	return mCapabilities & V4L2_CAP_RADIO;
#else
	return false;
#endif
}

bool V4L2_DeviceDescriptor::isVBIDev() const
{
	return mCapabilities & V4L2_CAP_VBI_CAPTURE;
}

