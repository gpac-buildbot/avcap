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
#include <sys/types.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "V4L2_ControlBase.h"
#include "V4L2_DeviceDescriptor.h"
#include "log.h"

#ifdef AVCAP_HAVE_V4L2
#include <linux/videodev2.h>
#else
#include <linux/videodev.h>
#endif

using namespace avcap;

// Construction & Destruction

V4L2_ControlBase::V4L2_ControlBase(V4L2_DeviceDescriptor *dd, struct v4l2_queryctrl* query):
	mDeviceDescriptor(dd),
	mId(query->id), 
	mName((const char*) query->name), 
	mDefaultValue(query->default_value), 
	mFlags(query->flags)
{
	getValue();
}

V4L2_ControlBase::~V4L2_ControlBase()
{
}

int V4L2_ControlBase::update()
{
	int res = 0;
	
	// Forward the changes to the driver.
	struct v4l2_control ctrl;
	memset(&ctrl, 0, sizeof(struct v4l2_control));
	ctrl.id = mId;
	ctrl.value = mValue;
	
	// try the standard way
	if((res = ioctl(mDeviceDescriptor->getHandle(), VIDIOC_S_CTRL, &ctrl)) != 0) {
		// and use the extended way if standard doesn't work
		struct v4l2_ext_controls	ctrls;
		struct v4l2_ext_control		ext_ctrl;
		memset(&ctrls, 0, sizeof(ctrls));
		memset(&ext_ctrl, 0, sizeof(ext_ctrl));
		
		ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
		ctrls.count = 1;
		ctrls.controls = &ext_ctrl;
		ext_ctrl.id = mId;
		ext_ctrl.value = mValue;
		
		res = ioctl(mDeviceDescriptor->getHandle(), VIDIOC_S_EXT_CTRLS, &ctrls);
	}
	
	return res; 
}

int V4L2_ControlBase::setValue(int value)
{
	mValue = value;
	return update();
}

int V4L2_ControlBase::getValue() const
{
	int value = 0;
	struct v4l2_control ctrl;
	memset(&ctrl, 0, sizeof(struct v4l2_control));
	ctrl.id = mId;
	
	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOC_G_CTRL, &ctrl) == 0) {
		value = ctrl.value;
	} else {
		struct v4l2_ext_controls	ctrls;
		struct v4l2_ext_control		ext_ctrl;

		memset(&ctrls, 0, sizeof(ctrls));
		memset(&ext_ctrl, 0, sizeof(ext_ctrl));
		
		ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
		ctrls.count = 1;
		ctrls.controls = &ext_ctrl;
		ext_ctrl.id = mId;
		
		if(ioctl(mDeviceDescriptor->getHandle(), VIDIOC_G_EXT_CTRLS, &ctrls) == 0) {
			value = ext_ctrl.value;		
		} else {
			logDebug("query extended control failed: ", errno);
		}
	}
	
	return value;
}


int V4L2_ControlBase::reset()
{
	mValue = mDefaultValue;
	return update();
}
