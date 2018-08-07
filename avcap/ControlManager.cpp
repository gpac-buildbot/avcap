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

#include "ControlManager.h"
#include "Control_avcap.h"
#include "DeviceDescriptor.h"

using namespace avcap;

// Construction & Destruction

ControlManager::ControlManager(DeviceDescriptor *dd): Manager<Control>(dd)
{
}

ControlManager::~ControlManager()
{
	for(ListType::iterator it = mControls.begin(); it != mControls.end(); it++)
		delete *it;
}


// Reset all controls in the list.

int ControlManager::resetAll()
{
	int res = 0;
	for (ListType::iterator it = mControls.begin(); it != mControls.end(); it++)
		res |= (*it)->reset();
		
	return res;
}

// Return the control with the specified name.

Control* ControlManager::getControl(const std::string& name)
{
	// iterate the list and find the control
	for(ListType::iterator it = mControls.begin(); it != mControls.end(); it++)
		if((*it)->getName() == name)
			return *it;
	
	// not control found	
	return 0;
}

// Return the control with the unique id.

Control* ControlManager::getControl(int id)
{
	// iterate the list and find the control
	for(ListType::iterator it = mControls.begin(); it != mControls.end(); it++)
		if((*it)->getId() == id)
			return *it;
			
	// no control found
	return 0;
}


