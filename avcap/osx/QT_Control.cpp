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


#include "QT_Control.h"
#include "QT_DeviceDescriptor.h"

using namespace avcap;

std::string QT_Control::mNames[7] = {"Brightness", "Blacklevel", "Whitelevel", "Hue", "Contrast", "Saturation", "Sharpness" };


// Construction & Destruction

QT_Control::QT_Control(QT_DeviceDescriptor* dd, Ctrl type):
	mDescriptor(dd),
	mType(type),
	mInterval(0, 65535, 1)
{
	mDefaultValue = getValue();
}

int QT_Control::setValue(int val)
{
int res = 0;
unsigned short sval = val;

	if(!mDescriptor || !mDescriptor->getDigitizer())
		return 0;

	switch(mType) {
		case BRIGHTNESS:
			if(VDSetBrightness(mDescriptor->getDigitizer(), &sval) != noErr)
				res = -1;
			break;

		case BLACKLEVEL:
			if(VDSetBlackLevelValue(mDescriptor->getDigitizer(), &sval) != noErr)
				res = -1;
			break;

		case WHITELEVEL:
			if(VDSetWhiteLevelValue(mDescriptor->getDigitizer(), &sval) != noErr)
				res = -1;
			break;
		
		case HUE:
			if(VDSetHue(mDescriptor->getDigitizer(), &sval) != noErr)
				res = -1;
			break;

		case CONTRAST:
			if(VDSetContrast(mDescriptor->getDigitizer(), &sval) != noErr)
				res = -1;
			break;
	
		case SATURATION:
			if(VDSetSaturation(mDescriptor->getDigitizer(), &sval) != noErr)
				res = -1;
			break;
			
		case SHARPNESS:
			if(VDSetSharpness(mDescriptor->getDigitizer(), &sval) != noErr)
				res = -1;
			break;

	}

	return res;
}

int QT_Control::getValue() const
{
unsigned short res = 0;

	if(!mDescriptor || !mDescriptor->getDigitizer())
		return 0;
		
	switch(mType) {
		case BRIGHTNESS:
			VDGetBrightness(mDescriptor->getDigitizer(), &res);
			break;

		case BLACKLEVEL:
			VDGetBlackLevelValue(mDescriptor->getDigitizer(), &res);
			break;

		case WHITELEVEL:
			VDGetWhiteLevelValue(mDescriptor->getDigitizer(), &res);
			break;
		
		case HUE:
			VDGetHue(mDescriptor->getDigitizer(), &res);
			break;

		case CONTRAST:
			VDGetContrast(mDescriptor->getDigitizer(), &res);
			break;

		case SATURATION:
			VDGetSaturation(mDescriptor->getDigitizer(), &res);
			break;

		case SHARPNESS:
			VDGetSharpness(mDescriptor->getDigitizer(), &res);
			break;
	}
	
	return res;
}

int QT_Control::reset()
{
	return setValue(mDefaultValue);
}

const std::string& QT_Control::getName() const
{
	return mNames[mType];
}



