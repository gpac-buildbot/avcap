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
#include <string>
#include <iostream>
#include <sys/ioctl.h>

#include "V4L2_Connector.h"
#include "V4L2_DeviceDescriptor.h"
#include "V4L2_Tuner.h"

#ifdef AVCAP_HAVE_V4L2
#include <linux/videodev2.h>
#else
#include <linux/videodev.h>
#endif



using namespace avcap;

// Construction & Destruction

V4L2_Connector::V4L2_Connector(V4L2_DeviceDescriptor *dd, int index, const std::string& name, int type, int audioset, int tuner_index):
	Connector(dd, index, name, type, audioset),
	mTuner(0)
{
	// is a tuner associated with the V4L2_Connector
	if(hasTuner())
	{
		// then query its properties
		struct v4l2_tuner	tuner;
		memset(&tuner, 0, sizeof(tuner));
		tuner.index = tuner_index;
	
		if(ioctl(mDeviceDescriptor->getHandle(), VIDIOC_G_TUNER, &tuner)==-1)
			return;

		// and create a Tuner object	
		mTuner = new V4L2_Tuner(dd, tuner_index, (const char*)tuner.name, tuner.type, tuner.capability, tuner.rangehigh, tuner.rangelow);
	}
}

V4L2_Connector::~V4L2_Connector()
{	
	delete mTuner;
}


