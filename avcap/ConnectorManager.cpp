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
#include <assert.h>

#include "ConnectorManager.h"
#include "Connector.h"
#include "DeviceDescriptor.h"

using namespace avcap;

// Construction & destruction

ConnectorManager::ConnectorManager(DeviceDescriptor *dd): 
	Manager<Connector>(dd)
{
}

ConnectorManager::~ConnectorManager()
{
	// clear all connector lists
	clearList(mVideoInputs);
	clearList(mVideoOutputs);
	clearList(mAudioInputs);
	clearList(mAudioOutputs);
}


void ConnectorManager::clearList(ConnectorManager::ListType& list)
{
	// delete the connector objects
	for(ListType::iterator it = list.begin(); it != list.end(); it++)
		delete *it;
}
