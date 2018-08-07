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

#include <sstream>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/types.h>

#include "avcap-config.h"
#ifndef AVCAP_HAVE_V4L2
#include <linux/videodev.h>

#include "V4L1_DeviceDescriptor.h"
#include "V4L1_Device.h"
#include "log.h"

using namespace avcap;

// Construction & Destruction

// construct a V4L2 device-descriptor
V4L1_DeviceDescriptor::V4L1_DeviceDescriptor(const std::string &name):
	mName(name), mHandle(-1), mType(0), mAudios(0), mChannels(0), mIsStreamingDev(false), mDevice(0)
{
	mBounds.minwidth 	= 0;
	mBounds.minheight 	= 0;
	mBounds.maxwidth 	= 0;
	mBounds.maxheight	= 0;

	// test whether it is a V4L1 device and query its capabilities
	mValid = queryCapabilities();
}

V4L1_DeviceDescriptor::~V4L1_DeviceDescriptor()
{
	close();
}

CaptureDevice* V4L1_DeviceDescriptor::getDevice()
{
	if(mDevice == 0)
		mDevice = new V4L1_Device(this);

	return mDevice;
}

int V4L1_DeviceDescriptor::open()
{
	// check if device-file is already open,
	// so multiple calls to open() will result in an error
	if(mHandle != -1)
		return -1;

	// Open the device file handle
	mHandle = ::open((const char*) mName.c_str(), O_RDWR /*| O_NONBLOCK*/ );

	if (mHandle < 0)
		return -1;


	return 0;
}

int V4L1_DeviceDescriptor::close()
{
	//  close the device file handle
	int res = 0;
	if(mHandle != -1)
		res = ::close(mHandle);

	mHandle = -1;

	delete mDevice;
	mDevice = 0;

	return  res;
}


bool V4L1_DeviceDescriptor::queryCapabilities()
{
	// test and query device capabilities. return true, if V4L1 device and false, if not.
	// temporary open
	int file = ::open((const char*) mName.c_str(), O_RDWR /*| O_NONBLOCK*/ );

	// failed
	if ( file < 0 )
		return false;

	struct video_capability caps;
	memset(&caps, 0, sizeof (video_capability));

	// query the capabilities
	if (ioctl(file, VIDIOCGCAP, &caps) < 0)
	{
		// its obviously no V4L1 device
		::close(file);
		return false;
	}

	// store the driver and device infos
	mDriver = (char*) caps.name;
	mCard 	= (char*) caps.name;
	mInfo	= (char*) caps.name;
	mVersion= 0;

	// remove trailing blanks, some driver names have the nasty
	// behaviour of having trailing blanks (e.g. Philips SPC700NC)
	// This is not very practical if you search a device by its driver name.
	while(mDriver.find_last_of(" ") == mDriver.size() - 1)
		mDriver = mDriver.substr(0, mDriver.size() - 1);

	mType = caps.type;
	mBounds.minwidth 	= caps.minwidth;
	mBounds.minheight 	= caps.minheight;
	mBounds.maxwidth 	= caps.maxwidth;
	mBounds.maxheight	= caps.maxheight;

	mAudios	= caps.audios;
	mChannels = caps.channels;

	struct 	video_mbuf vb;
	memset(&vb, 0, sizeof(vb));
	if(ioctl(file, VIDIOCGMBUF, &vb) == -1 || vb.frames < 1)
		mIsStreamingDev = false;
	else
		mIsStreamingDev = true;

	std::ostringstream res;
	res<<((mVersion >> 16) & 0xFF)<<"."<<((mVersion >> 8) & 0xFF)<<"."<<(mVersion & 0xFF);
	mVersionString = res.str();

	logDebug("V4L1_DeviceDescriptor::queryCapabilities(): Device found: "
			" Driver: " +	mDriver +
			" Card: " + mCard +
			" Bus: " + mInfo +
			" Version: " + getVersionString());

	::close (file);

	return true;
}

// return a textual representation of the drivers version number.
const std::string& V4L1_DeviceDescriptor::getVersionString() const
{
	return mVersionString;
}

const std::string& V4L1_DeviceDescriptor::getName() const
{
	return mName;
}


// Return true if it is a V4L1 device, false otherwise.

bool V4L1_DeviceDescriptor::isAVDev() const
{
	return mValid;
}

// Linux-specific evaluation of the capabilities.

bool V4L1_DeviceDescriptor::isVideoCaptureDev() const
{
	return mType & VID_TYPE_CAPTURE;
}

bool V4L1_DeviceDescriptor::isTuner() const
{
	return mType & VID_TYPE_TUNER;
}

bool V4L1_DeviceDescriptor::isAudioDev() const
{
	return mAudios > 0;
}

bool V4L1_DeviceDescriptor::isOverlayDev() const
{
	return mType & VID_TYPE_OVERLAY;
}

bool V4L1_DeviceDescriptor::isRWDev () const
{
	return true;
}

bool V4L1_DeviceDescriptor::isAsyncIODev() const
{
	return false;
}

bool V4L1_DeviceDescriptor::isStreamingDev() const
{
	return mIsStreamingDev;
}

bool V4L1_DeviceDescriptor::isRadioDev() const
{
	return false;
}

bool V4L1_DeviceDescriptor::isVBIDev() const
{
	return mType & VID_TYPE_TELETEXT;
}

#endif

