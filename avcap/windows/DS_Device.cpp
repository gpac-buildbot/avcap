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

#include "DS_Device.h"
#include "DS_ConnectorManager.h"
#include "DS_ControlManager.h"
#include "DS_FormatManager.h"
#include "DS_VidCapManager.h"

#include "HelpFunc.h"

using namespace avcap;

//	Construction & destruction

DS_Device::DS_Device(DS_DeviceDescriptor *dd) :
	mVidCapMgr(0), mConnectorMgr(0), mControlMgr(0), mFormatMgr(0), mDSDeviceDescriptor(dd)
{
	open();
}

DS_Device::~DS_Device()
{
	close();
}

int DS_Device::open()
{
	if (!mDSDeviceDescriptor)
		return -1;

	// Instantiate the managers
	mConnectorMgr = new DS_ConnectorManager(mDSDeviceDescriptor);
	mControlMgr = new DS_ControlManager(mDSDeviceDescriptor);
	mFormatMgr = new DS_FormatManager(mDSDeviceDescriptor);
	mVidCapMgr = new DS_VidCapManager(mDSDeviceDescriptor, mFormatMgr);

	// Query components
	mConnectorMgr->query();
	mControlMgr->query();
	mFormatMgr->query();

	// Init
	mVidCapMgr->init();

	return 0;
}

int DS_Device::close()
{
	// Destroy objects and close the device
	if (mVidCapMgr) {
		mVidCapMgr->destroy();
	}

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
