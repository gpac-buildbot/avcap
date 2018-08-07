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

#include <iostream>
#include <assert.h>

#include "V4L2_Device.h"
#include "V4L2_ConnectorManager.h"
#include "V4L2_ControlManager.h"
#include "V4L2_FormatManager.h"
#include "V4L2_VidCapManager.h"
#include "log.h"

using namespace avcap;

//	Construction & destruction

V4L2_Device::V4L2_Device(V4L2_DeviceDescriptor *dd):
	mVidCapMgr(0),
	mConnectorMgr(0),
	mControlMgr(0),
	mFormatMgr(0),
	mDeviceDescriptor(dd)
{
	open();
}

V4L2_Device::~V4L2_Device()
{
	close();
}

int V4L2_Device::open()
{
	logDebug("V4L2_Device::open(): " + mDeviceDescriptor->getCard());

	// assert that the device is valid
	assert(mDeviceDescriptor != 0);

	// instantiate the managers
	mConnectorMgr = new V4L2_ConnectorManager(mDeviceDescriptor);
	mControlMgr = new V4L2_ControlManager(mDeviceDescriptor);
	mFormatMgr = new V4L2_FormatManager(mDeviceDescriptor);
	mVidCapMgr = new V4L2_VidCapManager(mDeviceDescriptor, mFormatMgr);

	// query components
	mConnectorMgr->query();
	mControlMgr->query();

	mFormatMgr->query();

	// init
	mVidCapMgr->init();

	return 0;
}

int V4L2_Device::close()
{
	logDebug("V4L2_Device::close(): " + mDeviceDescriptor->getCard());

	// destroy objects and close the device
	if(mVidCapMgr)
		mVidCapMgr->destroy();

	delete mVidCapMgr;
	delete mConnectorMgr;
	delete mControlMgr;
	delete mFormatMgr;

	mVidCapMgr = 0;
	mConnectorMgr = 0;
	mControlMgr = 0;
	mFormatMgr = 0;

	return 0;
}

