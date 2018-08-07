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


#include <sys/ioctl.h>
#include <iostream>
#include <assert.h>
#include <string.h>

#include "avcap-config.h"
#ifndef AVCAP_HAVE_V4L2

#include "V4L1_ConnectorManager.h"
#include "V4L1_DeviceDescriptor.h"
#include "V4L1_Connector.h"


using namespace avcap;

// Construction & destruction

V4L1_ConnectorManager::V4L1_ConnectorManager(V4L1_DeviceDescriptor *dd): 
	ConnectorManager((DeviceDescriptor*) dd),
	mCurrentVideoInput(0)
{
}

V4L1_ConnectorManager::~V4L1_ConnectorManager()
{
}

void V4L1_ConnectorManager::query()
{
#ifdef DEBUG
	std::cout<<std::endl;
#endif

	assert (mDeviceDescriptor != 0);

	V4L1_DeviceDescriptor *V4L1_dd = dynamic_cast<V4L1_DeviceDescriptor*>(mDeviceDescriptor);
	
	if(!V4L1_dd)
		return;
		 
	// query video input connectors	
	for(int index = 0; index < V4L1_dd->getChannels(); index++) {
		struct video_channel vc;
		memset(&vc, 0, sizeof(vc));
		vc.channel = index;
		
		if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCGCHAN, &vc) == -1)
			return;
			
		// create a new connector-object
		V4L1_Connector* con = new V4L1_Connector(V4L1_dd, vc.channel, vc.name, vc.type);
		mVideoInputs.push_back(con);
	}
	
	if(mVideoInputs.size())
		setVideoInput(mVideoInputs.front());
}


Connector* V4L1_ConnectorManager::findByIndex(const ListType& l, int index)
{
	// Find a Connector by its unique index.
	Connector	*res = 0;

	// iterate the list	
	for(ListType::const_iterator it = l.begin(); it != l.end(); it++) {
		res = *it;
		if(res->getIndex() == index)
			return res;
	} 
	
	return 0;
}

Connector* V4L1_ConnectorManager::getVideoInput()
{
	// Return the current video input connector.
	assert (mDeviceDescriptor != 0);
	int index = 0;
	
	
	// get the current input index from the driver and find the connector object
	if (-1 != ioctl (mDeviceDescriptor->getHandle() , VIDIOC_G_INPUT, &index)) 
		return findByIndex (mVideoInputs, index);
	
	return 0;
}


int V4L1_ConnectorManager::setVideoInput(Connector* c)
{
	// Set the current video input.
	assert (mDeviceDescriptor != 0);

	struct video_channel vc;
	memset(&vc, 0, sizeof(vc));
	vc.channel = c->getIndex();
											
	int res = ioctl(mDeviceDescriptor->getHandle(), VIDIOCSCHAN, &vc);
	if(res != -1) {
		mCurrentVideoInput = c;
		return 0;
	}
	
	return -1;
}

#endif


