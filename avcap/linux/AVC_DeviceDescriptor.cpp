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

#ifdef HAS_AVC_SUPPORT

#include <sstream>

#include "AVC_DeviceDescriptor.h"
#include "AVC_Device.h"

using namespace avcap;

int AVC_DeviceDescriptor::mDevCount = 1;

// Construction & Destruction

AVC_DeviceDescriptor::AVC_DeviceDescriptor(const octlet_t guid):
	mGUID(guid), mDevice(0)
{
	char buf[256];
	sprintf(buf, "AV/C device on IEEE 1394, GUID 0x%08x%08x\n",
						(quadlet_t) (mGUID>>32), (quadlet_t) (mGUID & 0xffffffff));
	mInfo = buf;

	sprintf(buf, "0x%08x%08x",
						(quadlet_t) (mGUID>>32), (quadlet_t) (mGUID & 0xffffffff));
	mGUIDString = buf;

	sprintf(buf, "AV/C_%d", mDevCount++);
	mName = buf;
	mDriver = "IEEE 1394 Subsystem";
}

AVC_DeviceDescriptor::~AVC_DeviceDescriptor()
{
	close();
}

int AVC_DeviceDescriptor::open()
{
	return 0;
}

int AVC_DeviceDescriptor::close()
{
	delete mDevice;
	mDevice = 0;

	return 0;
}

CaptureDevice* AVC_DeviceDescriptor::getDevice()
{
	// create the AVC-device
	if(mDevice == 0)
		mDevice = new AVC_Device(this);

	return mDevice;
}

#endif
