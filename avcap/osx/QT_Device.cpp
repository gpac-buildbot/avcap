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

#include "QT_DeviceDescriptor.h"
#include "QT_Device.h"
#include "QT_VidCapManager.h"
#include "QT_ConnectorManager.h"
#include "QT_ControlManager.h"
#include "log.h"

using namespace avcap;

//	Construction & destruction

QT_Device::QT_Device(QT_DeviceDescriptor *dd):
	mDeviceDescriptor(dd),
	mFormatMgr(0),
	mVidCapMgr(0),
	mConnectorMgr(0),
	mControlMgr(0)
{
	open();
}

QT_Device::~QT_Device()
{
	close();
}

int QT_Device::open()
{
	logDebug("QT_Device::open(): " + mDeviceDescriptor->getName());

	// assert that the device is valid
	assert(mDeviceDescriptor != 0);

	// try to open the device
	if (mDeviceDescriptor->open() < 0)
		return -1;

	// create the QT-Managers and call their query-method
	mFormatMgr = new QT_FormatManager(mDeviceDescriptor);
	mFormatMgr->query();

	mConnectorMgr = new QT_ConnectorManager(mDeviceDescriptor);
	mConnectorMgr->query();

	mControlMgr = new QT_ControlManager(mDeviceDescriptor);
	mControlMgr->query();

	mVidCapMgr = new QT_VidCapManager(mDeviceDescriptor, mFormatMgr, 8);

	return mVidCapMgr->init();
}

int QT_Device::close()
{
	logDebug("QT_Device::close(): " + mDeviceDescriptor->getName());

	// stop capture and destroy the capture-manager
	if(mVidCapMgr) {
		mVidCapMgr->stopCapture();
		mVidCapMgr->destroy();
		delete mVidCapMgr;
		mVidCapMgr = 0;
	}

	// delete the other QT-managers
	delete mConnectorMgr;
	mConnectorMgr = 0;

	delete mControlMgr;
	mControlMgr = 0;

	delete mFormatMgr;
	mFormatMgr = 0;

	return 0;
}

