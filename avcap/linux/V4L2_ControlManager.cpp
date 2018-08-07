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


#include <errno.h>
#include <string.h>
#include <assert.h>
#include <iostream>
#include <sys/ioctl.h>
#include <linux/types.h>

#include "Control_avcap.h"
#include "V4L2_ControlManager.h"
#include "V4L2_MenuControl.h"
#include "V4L2_IntControl.h"
#include "V4L2_BoolControl.h"
#include "V4L2_ButtonControl.h"
#include "V4L2_CtrlClassControl.h"
#include "V4L2_DeviceDescriptor.h"

#ifdef AVCAP_HAVE_V4L2
#include <linux/videodev2.h>
#else
#include <linux/videodev.h>
#endif


using namespace avcap;

// Construction & Destruction

V4L2_ControlManager::V4L2_ControlManager(V4L2_DeviceDescriptor *dd):
	ControlManager((DeviceDescriptor*) dd)
{
}

V4L2_ControlManager::~V4L2_ControlManager()
{
}

void V4L2_ControlManager::query()
{
	// query controls with the extended mechanism
	if(!queryExtended()) {
		// and if this fails, try to query the standard way
		// query the standard controls
		query(V4L2_CID_BASE, V4L2_CID_LASTP1);
		// and the private controls
		query(V4L2_CID_PRIVATE_BASE, V4L2_CID_PRIVATE_BASE + 256);
	}
}

bool V4L2_ControlManager::queryExtended()
{
	bool supported = false;

	assert (mDeviceDescriptor != 0);

	V4L2_DeviceDescriptor* v4l2_dd = dynamic_cast<V4L2_DeviceDescriptor*>(mDeviceDescriptor);
	if(!v4l2_dd)
		return false;

	struct v4l2_queryctrl	query;
	memset(&query, 0, sizeof(v4l2_queryctrl));

	query.id = V4L2_CTRL_FLAG_NEXT_CTRL;

	while (0 == ioctl (mDeviceDescriptor->getHandle(), VIDIOC_QUERYCTRL, &query)) {
		supported = true;

		Control *c = 0;
		switch(query.type)
		{
			case V4L2_CTRL_TYPE_MENU:
				c = new V4L2_MenuControl(v4l2_dd, &query);
			break;
			case V4L2_CTRL_TYPE_INTEGER:
				c = new V4L2_IntControl(v4l2_dd, &query);
			break;
			case V4L2_CTRL_TYPE_BOOLEAN:
				c = new V4L2_BoolControl(v4l2_dd, &query);
			break;
			case V4L2_CTRL_TYPE_BUTTON:
				c = new V4L2_ButtonControl(v4l2_dd, &query);
			break;

			case V4L2_CTRL_TYPE_CTRL_CLASS:
				c = new V4L2_CtrlClassControl(v4l2_dd, &query);
			break;


			case V4L2_CTRL_TYPE_INTEGER64:
			//	c = new V4L2_Int64Control(v4l2_dd, &query);
				std::cerr<<"V4L2_CTRL_TYPE_INTEGER64-control found\n";
			break;

			default:
			break;
		}

		// and store it in the list
		if(c != 0)	mControls.push_back(c);
		query.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}

	return supported;
}

void V4L2_ControlManager::query(int start_id, int end_id)
{
	// Query a specific index range.

#ifdef DEBUG
	std::cout<<std::endl;
#endif

	assert (mDeviceDescriptor != 0);

	V4L2_DeviceDescriptor* v4l2_dd = dynamic_cast<V4L2_DeviceDescriptor*>(mDeviceDescriptor);
	if(!v4l2_dd)
		return;

	// query controls
	for(int id = start_id; id < end_id; id++) {
		// init the query struct
		struct v4l2_queryctrl	query;
		memset(&query, 0, sizeof(v4l2_queryctrl));
		query.id = id;

		// do ioctl and abort if the ctrl is out of index bounds
		if (ioctl(mDeviceDescriptor->getHandle(), VIDIOC_QUERYCTRL, &query) == -1) {
				break;
		}

		// try next ctrl if the ctrl is disabled
		if (query.flags & V4L2_CTRL_FLAG_DISABLED)
			continue;

		// create a control of the correct type
		Control *c = 0;
		switch(query.type)
		{
			case V4L2_CTRL_TYPE_MENU:
				c = new V4L2_MenuControl(v4l2_dd, &query);
			break;
			case V4L2_CTRL_TYPE_INTEGER:
				c = new V4L2_IntControl(v4l2_dd, &query);
			break;
			case V4L2_CTRL_TYPE_BOOLEAN:
				c = new V4L2_BoolControl(v4l2_dd, &query);
			break;
			case V4L2_CTRL_TYPE_BUTTON:
				c = new V4L2_ButtonControl(v4l2_dd, &query);
			break;

			default:
			break;
		}

		// and store it in the list
		if(c != 0) mControls.push_back(c);

#ifdef DEBUG
	std::cout<<"V4L2_ControlManager::queryControls(): Control: "<< query.name<<", ID: "
	<<query.id<<", type: "<<query.type<<", min: "<<query.minimum<<", max: "<<query.maximum<<", step: "<<query.step
	<<", default: "<<query.default_value<<"\n";
#endif
	}

#ifdef DEBUG
	std::cout<<"\n";
#endif
}




