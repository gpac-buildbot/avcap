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
#include <linux/types.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "avcap-config.h"
#ifndef AVCAP_HAVE_V4L2
#include <linux/videodev.h>

#include "V4L1_Control.h"
#include "V4L1_DeviceDescriptor.h"


using namespace avcap;

std::string V4L1_Control::mNames[6] = {"Brightness", "Hue", "Colour", "Contrast", "Whiteness", "Depth" };


// Construction & Destruction
V4L1_Control::V4L1_Control(V4L1_DeviceDescriptor* dd, Ctrl type, __u16 def):
	mDescriptor(dd),
	mType(type),
	mDefaultValue(def),
	mInterval(0, 65535, 1)
{
}

int V4L1_Control::setValue(int val)
{
	struct video_picture vp;
	memset(&vp, 0, sizeof(vp));
	if(ioctl(mDescriptor->getHandle(), VIDIOCGPICT, &vp) == -1)
		return -1;
	
	switch(mType) {
		case BRIGHTNESS:
			vp.brightness = val;
			break;
		
		case HUE:
			vp.hue = val;
			break;
		
		case COLOUR:
			vp.colour = val;
			break;

		case CONTRAST:
			vp.contrast = val;
			break;
	
		case WHITENESS:
			vp.whiteness = val;
			break;
	
		case DEPTH:
			vp.depth = val;
			break;
	}

	return ioctl(mDescriptor->getHandle(), VIDIOCSPICT, &vp);
}

int V4L1_Control::getValue() const
{
int	res = -1;

	struct video_picture vp;
	memset(&vp, 0, sizeof(vp));
	if(ioctl(mDescriptor->getHandle(), VIDIOCGPICT, &vp) == -1)
		return -1;
	
	switch(mType) {
		case BRIGHTNESS:
			res = vp.brightness;
			break;
		
		case HUE:
			res = vp.hue;
			break;
		
		case COLOUR:
			res = vp.colour;
			break;

		case CONTRAST:
			res = vp.contrast;
			break;
	
		case WHITENESS:
			res = vp.whiteness;
			break;
	
		case DEPTH:
			res = vp.depth;
			break;
	}
	
	return res;
}

int V4L1_Control::reset()
{
	return setValue(mDefaultValue);
}

const std::string& V4L1_Control::getName() const
{
	return mNames[mType];
}


#endif

