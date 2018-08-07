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

#include "QT_ControlManager.h"
#include "QT_Control.h"
#include "QT_DeviceDescriptor.h"

using namespace avcap;

// Construction & Destruction

QT_ControlManager::QT_ControlManager(QT_DeviceDescriptor *dd): 
	ControlManager((DeviceDescriptor*) dd)
{
}

QT_ControlManager::~QT_ControlManager()
{
}

// Query for available controls.

void QT_ControlManager::query()
{
	QT_DeviceDescriptor* qt_dd = dynamic_cast<QT_DeviceDescriptor*>(mDeviceDescriptor);
	
	if(qt_dd) {
		mControls.push_back(new QT_Control(qt_dd, QT_Control::BRIGHTNESS));
		mControls.push_back(new QT_Control(qt_dd, QT_Control::BLACKLEVEL));
		mControls.push_back(new QT_Control(qt_dd, QT_Control::WHITELEVEL));
		mControls.push_back(new QT_Control(qt_dd, QT_Control::CONTRAST));
		mControls.push_back(new QT_Control(qt_dd, QT_Control::HUE));
		mControls.push_back(new QT_Control(qt_dd, QT_Control::SATURATION));
		mControls.push_back(new QT_Control(qt_dd, QT_Control::SHARPNESS));
	}
}





