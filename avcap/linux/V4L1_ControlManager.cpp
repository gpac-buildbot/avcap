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
#include <assert.h>
#include <iostream>
#include <sys/ioctl.h>
#include <linux/types.h>

#include "avcap-config.h"
#ifndef AVCAP_HAVE_V4L2
#include <linux/videodev.h>

#include "V4L1_ControlManager.h"
#include "V4L1_Control.h"
#include "V4L1_DeviceDescriptor.h"

using namespace avcap;

// Construction & Destruction

V4L1_ControlManager::V4L1_ControlManager(V4L1_DeviceDescriptor *dd): 
	ControlManager((DeviceDescriptor*) dd)
{
}

V4L1_ControlManager::~V4L1_ControlManager()
{
}

void V4L1_ControlManager::query()
{
	// Query for available controls.
	struct video_picture vp;
	memset(&vp, 0, sizeof(vp));
	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCGPICT, &vp) == -1)
		return;
	
	// create standard controls
	V4L1_DeviceDescriptor* v4l1_dd = dynamic_cast<V4L1_DeviceDescriptor*>(mDeviceDescriptor);
	
	if(v4l1_dd) {
		mControls.push_back(new V4L1_Control(v4l1_dd, V4L1_Control::BRIGHTNESS, vp.brightness));
		mControls.push_back(new V4L1_Control(v4l1_dd, V4L1_Control::HUE, vp.hue));
		mControls.push_back(new V4L1_Control(v4l1_dd, V4L1_Control::COLOUR, vp.colour));
		mControls.push_back(new V4L1_Control(v4l1_dd, V4L1_Control::CONTRAST, vp.contrast));
		mControls.push_back(new V4L1_Control(v4l1_dd, V4L1_Control::WHITENESS, vp.whiteness));
		mControls.push_back(new V4L1_Control(v4l1_dd, V4L1_Control::DEPTH, vp.depth));
	}
}



#endif


