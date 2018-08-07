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

#include "V4L2_ConnectorManager.h"
#include "V4L2_DeviceDescriptor.h"
#include "V4L2_Connector.h"
#include "log.h"

#ifdef AVCAP_HAVE_V4L2
#include <linux/videodev2.h>
#else
#include <linux/videodev.h>
#endif

using namespace avcap;

// Construction & destruction

V4L2_ConnectorManager::V4L2_ConnectorManager(V4L2_DeviceDescriptor *dd): 
	ConnectorManager((DeviceDescriptor*) dd)
{
}

V4L2_ConnectorManager::~V4L2_ConnectorManager()
{
}

void V4L2_ConnectorManager::query()
{
	assert (mDeviceDescriptor != 0);

	V4L2_DeviceDescriptor *v4l2_dd = dynamic_cast<V4L2_DeviceDescriptor*>(mDeviceDescriptor);
	
	if(!v4l2_dd)
		return;
		 
	// query video input connectors	
	for(int index = 0;; index++) {
		struct v4l2_input	c;
		memset(&c, 0, sizeof (v4l2_input));
		c.index = index;
		
		if (ioctl(mDeviceDescriptor->getHandle(), VIDIOC_ENUMINPUT, &c) < 0)
			break;

		// create connector and store it in the list
		V4L2_Connector	*con = new V4L2_Connector(v4l2_dd, c.index, 
			(const char*) c.name, c.type, c.audioset, c.tuner);
			
		mVideoInputs.push_back(con);
		logDebug("V4L2_ConnectorManager::query(): Video Output found: Name: "+ con->getName());
	}
	
	// query video output connectors		
	for(int index = 0;; index++) {
		struct v4l2_output	c;
		memset(&c, 0, sizeof (v4l2_output));
		c.index = index;

		if (ioctl(mDeviceDescriptor->getHandle(), VIDIOC_ENUMOUTPUT, &c) < 0)
			break;
			
		// create connector and store it in the list
		V4L2_Connector	*con = new V4L2_Connector(v4l2_dd, c.index, (const char*)c.name, c.type, c.audioset);
		mVideoOutputs.push_back(con);
		logDebug("V4L2_ConnectorManager::query(): Video Output found: Name: "+ con->getName());
	}
	
#ifdef VIDIOC_ENUMAUDIO		
	// query audio input connectors	
	for(int index = 0;; index++)
	{
		struct v4l2_audio	c;
		memset(&c, 0, sizeof (v4l2_audio));
		c.index = index;

		if (ioctl(mDeviceDescriptor->getHandle(), VIDIOC_ENUMAUDIO, &c) < 0)
			break;
			
		// create connector and store it in the list
		V4L2_Connector	*con = new V4L2_Connector( v4l2_dd, c.index, (const char*)c.name );
		mAudioInputs.push_back(con);
		logDebug("V4L2_ConnectorManager::query(): Video Output found: Name: "+ con->getName());
	}
#endif
		
#ifdef VIDIOC_ENUMAUDOUT
	// query audio output connectors	
	for(int index = 0;; index++)
	{
		struct v4l2_audioout	c;
		memset(&c, 0, sizeof (v4l2_audioout));
		c.index = index;

		if (ioctl(mDeviceDescriptor->getHandle(), VIDIOC_ENUMAUDOUT, &c) < 0)
			break;
			
		// create connector and store it in the list
		V4L2_Connector	*con = new V4L2_Connector(v4l2_dd, c.index, (const char*)c.name );
		mAudioOutputs.push_back(con);
		logDebug("V4L2_ConnectorManager::query(): Video Output found: Name: "+ con->getName());
	}
#endif
}

// Find a Connector by its unique index.

Connector* V4L2_ConnectorManager::findByIndex(const ListType& l, int index)
{
	Connector	*res = 0;

	// iterate the list	
	for(ListType::const_iterator it = l.begin(); it != l.end(); it++) {
		res = *it;
		if(res->getIndex() == index)
			return res;
	} 
	
	return 0;
}


Connector* V4L2_ConnectorManager::getVideoInput()
{
	// Return the current video input connector.

	assert (mDeviceDescriptor != 0);
	int index = 0;
	
	// get the current input index from the driver and find the connector object
	if (-1 != ioctl (mDeviceDescriptor->getHandle() , VIDIOC_G_INPUT, &index)) 
		return findByIndex (mVideoInputs, index);
	
	return 0;
}


int V4L2_ConnectorManager::setVideoInput(Connector* c)
{
	// Set the current video input.

	assert (mDeviceDescriptor != 0);

	int index = c->getIndex();
	int res = ioctl(mDeviceDescriptor->getHandle() , VIDIOC_S_INPUT, &index);

	return res;
}


Connector* V4L2_ConnectorManager::getAudioInput()
{
	// Return the current audio input connector.

	assert (mDeviceDescriptor != 0);
	struct v4l2_audio ain;
	memset(&ain, 0, sizeof(v4l2_audio));

	// get the current input index from the driver and find the connector object	
	if (-1 != ioctl (mDeviceDescriptor->getHandle() , VIDIOC_G_AUDIO, &ain)) 
		return findByIndex (mAudioInputs, ain.index);
		
	return 0;
}

int V4L2_ConnectorManager::setAudioInput(Connector* c)
{
	// Set the current audio input.

	assert (mDeviceDescriptor != 0);
	struct v4l2_audio ain;
	memset(&ain, 0, sizeof(v4l2_audio));
	ain.index = c->getIndex();
	
	return ioctl(mDeviceDescriptor->getHandle() , VIDIOC_G_AUDIO, &ain);
}

Connector* V4L2_ConnectorManager::getVideoOutput()
{
	// Return the current video output connector.

	assert (mDeviceDescriptor != 0);
	int index = 0;
	
	// get the current input index from the driver and find the connector object	
	if (-1 != ioctl (mDeviceDescriptor->getHandle() , VIDIOC_G_OUTPUT, &index)) 
		return findByIndex (mVideoOutputs, index);
	
	return 0;
}


int V4L2_ConnectorManager::setVideoOutput(Connector* c)
{
	// Set the current video output.

	assert (mDeviceDescriptor != 0);
	int index = c->getIndex();
	return ioctl(mDeviceDescriptor->getHandle() , VIDIOC_S_OUTPUT, &index);
}

Connector* V4L2_ConnectorManager::getAudioOutput()
{
	// Return the current audio output connector.

	assert (mDeviceDescriptor != 0);
	struct v4l2_audioout aout;	
	memset(&aout, 0, sizeof(v4l2_audioout));
	
	// get the current input index from the driver and find the connector object
	if (-1 != ioctl (mDeviceDescriptor->getHandle() , VIDIOC_G_AUDOUT, &aout)) 
		return findByIndex (mAudioOutputs, aout.index);
		
	return 0;
}

int V4L2_ConnectorManager::setAudioOutput(Connector* c)
{
	// Set the current audio output.

	assert (mDeviceDescriptor != 0);

	struct v4l2_audioout aout;
	memset(&aout, 0, sizeof(v4l2_audioout));
	aout.index = c->getIndex();
	
	return ioctl(mDeviceDescriptor->getHandle() , VIDIOC_S_AUDOUT, &aout);
}

