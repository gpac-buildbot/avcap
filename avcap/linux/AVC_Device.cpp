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

#include <iostream>
#include <assert.h>

#include "AVC_DeviceDescriptor.h"
#include "AVC_Device.h"
#include "AVC_VidCapManager.h"
#include "AVC_ConnectorManager.h"
#include "AVC_ControlManager.h"

using namespace avcap;

//	Construction & destruction

AVC_Device::AVC_Device(AVC_DeviceDescriptor *dd):
	mDeviceDescriptor(dd),
	mFormatMgr(0),
	mVidCapMgr(0),
	mConnectorMgr(0),
	mControlMgr(0)
{
	open();
}

AVC_Device::~AVC_Device()
{
	close();
}

int AVC_Device::open()
{
#ifdef DEBUG
	std::cout<<"AVC_Device::open(): "<<mDeviceDescriptor->getName()<<std::endl;
#endif

	// assert that the device is valid
	assert(mDeviceDescriptor != 0);

	// assert we have a valid handle
	assert(mDeviceDescriptor->getGUID() != 0);

	// create and initialize the managers
	mFormatMgr = new AVC_FormatManager(mDeviceDescriptor);
	mFormatMgr->query();

	mConnectorMgr = new AVC_ConnectorManager(mDeviceDescriptor);
	mConnectorMgr->query();

	mControlMgr = new AVC_ControlManager(mDeviceDescriptor);
	mControlMgr->query();

	mVidCapMgr = new AVC_VidCapManager(mDeviceDescriptor, mFormatMgr);

	return mVidCapMgr->init();
}

int AVC_Device::close()
{
#ifdef DEBUG
	std::cout<<"AVC_Device::close(): "<<mDeviceDescriptor->getCard()<<std::endl;
#endif

	// stop capture and delete the managers
	if(mVidCapMgr) {
		mVidCapMgr->stopCapture();
		mVidCapMgr->destroy();
		delete mVidCapMgr;
		mVidCapMgr = 0;
	}

	delete mConnectorMgr;
	mConnectorMgr = 0;

	delete mControlMgr;
	mControlMgr = 0;

	delete mFormatMgr;
	mFormatMgr = 0;

	return 0;
}

#endif

