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


#include <string>
#include <iostream>

#include "HelpFunc.h"
#include "DS_Connector.h"
#include "DS_DeviceDescriptor.h"
#include "DS_Tuner.h"

using namespace avcap;

// Construction & Destruction

DS_Connector::DS_Connector(DS_DeviceDescriptor *dd, int index,
		const std::string& name, int type, int audioset, int tuner) :
	Connector(dd, index, name, type, audioset), mDSDeviceDescriptor(dd)
{
	mTuner = 0;
	
	// Is a tuner associated with the connector
	if (hasTuner()) {
		if (mType & CAP_RADIO) {
			mTuner = new DS_Tuner(dd, 0, "Radio tuner",type,0,0,0);
		}

		if (mType & CAP_TUNER) {
			mTuner = new DS_Tuner(dd, 0, "TV tuner",type,0,0,0);
		}
	}
}

DS_Connector::~DS_Connector()
{
	delete mTuner;
}
