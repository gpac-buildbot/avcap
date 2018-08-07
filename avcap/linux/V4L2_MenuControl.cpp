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
#include <sys/ioctl.h>
#include <linux/types.h>

#include "V4L2_MenuControl.h"
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

V4L2_MenuControl::V4L2_MenuControl(V4L2_DeviceDescriptor *dd, struct v4l2_queryctrl* query):
	mControlBase(dd, query),
	mDeviceDescriptor(dd)
{
	// query items
	queryMenuItems();
}

V4L2_MenuControl::~V4L2_MenuControl()
{
	// delet all items
	for(ItemList::iterator it = mMenuItems.begin(); it != mMenuItems.end(); it++)
		delete *it;
}

void V4L2_MenuControl::queryMenuItems()
{
	// Query available menu items.

	// init the query struct
	struct v4l2_querymenu	query_menu;
	memset(&query_menu, 0, sizeof(v4l2_querymenu));
	query_menu.id 	 = getId();
	
	// query the items
	for(query_menu.index = 0;; query_menu.index++)
	{
		// do the ioctl
		if (ioctl(mDeviceDescriptor->getHandle(), VIDIOC_QUERYMENU, &query_menu) == -1)
			break;
		
		// create a new menu item
		MenuItem *item = new MenuItem((const char*)query_menu.name, query_menu.index);
		
		// add the item to the list
		mMenuItems.push_back(item);
		logDebug("\tMenuItem: " + item->name);
	}
}

