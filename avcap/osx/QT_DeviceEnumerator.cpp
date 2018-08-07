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
#include <sstream>

#include "QT_DeviceEnumerator.h"
#include "log.h"

using namespace avcap;

QT_DeviceEnumerator::QT_DeviceEnumerator():
	mDeviceList(0)
{
}

QT_DeviceEnumerator::~QT_DeviceEnumerator()
{
	close();
}

int QT_DeviceEnumerator::open()
{
	// open the underlying grabber without specifying a concrete device and input  
	int res = QT_DeviceDescriptor::open();

	if(res)
		return res;
		
	OSErr err;
	
	// get the device-list including their inputs for the video-channel
	if((err = SGGetChannelDeviceList(getChannel(), sgDeviceListIncludeInputs, &mDeviceList))) {
		logDebug("QT_DeviceEnumerator: SGGetChannelDeviceList failed", err);
		return -1;
	}	

	return 0;
}

int QT_DeviceEnumerator::close()
{
	// dispose device-list
	if(mDeviceList)
		SGDisposeDeviceList(getGrabber(), mDeviceList);

	return QT_DeviceDescriptor::close();
}

int QT_DeviceEnumerator::findDevices(DeviceCollector::DeviceList& dev_list)
{
	// be sure, the device is open
	if(QT_DeviceDescriptor::open() != 0)	
		return -1;

	// iterate through the device-list
	SGDeviceListPtr	plist = *mDeviceList;
	for(int i = 0; i < plist->count; ++i ) {
		
		// ignore devices without inputs
		if(!plist->entry[i].inputs)
			continue;

		// and enumerate all inputs
		SGDeviceInputListPtr pinput_list = *(plist->entry[i].inputs);
		for (int j = 0; j < pinput_list->count; ++j) {
#ifdef DEBUG
			std::cout<<"QT_DeviceEnumerator: Found capture device: "<<i<<
				", input: "<<j<<
				", name: "<<plist->entry[i].name<<
				", input: "<<pinput_list->entry[j].name<<std::endl;
#endif
			// create a new descriptor for the found capture-device
			QT_DeviceDescriptor *dd = new QT_DeviceDescriptor(i, j, 
				(const char*) pinput_list->entry[j].name, 
				(const char*) plist->entry[i].name,
				getGrabber(), getChannel());
			
			// and add it to the device-list
			dev_list.push_back(dd);
		}
	}

	return 0;
}

