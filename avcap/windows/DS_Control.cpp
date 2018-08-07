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

#include "DS_Control.h"
#include "DS_DeviceDescriptor.h"
#include "HelpFunc.h"

using namespace avcap;

// Construction & Destruction

DS_Control::DS_Control(DS_DeviceDescriptor *dd, Query_Ctrl* query) :
	mDSDeviceDescriptor(dd), mId(query->id), mValue(-1), mName(query->name),
			mDefaultValue(query->default_value), mFlags(query->flags),
			mDShowInterfaceType(query->DShowInterfaceType),
			mProperty(query->property), mInitialized(false)
{
	retrieveValue();
}

DS_Control::~DS_Control()
{
}

int DS_Control::update()
{
	// Forward the changes to the driver.
	switch (mDShowInterfaceType)
	{
	case IAM_VIDEOPROCAMP:
	{
		QzCComPtr<IAMVideoProcAmp> VideoProcAmp;
		if (FAILED(((IBaseFilter*)mDSDeviceDescriptor->getHandle())->QueryInterface(
				IID_IAMVideoProcAmp, (void**) &VideoProcAmp))) {
			return -1;
		}
		
		if (FAILED(VideoProcAmp->Set(mProperty, mValue, mFlags))) {
			return -1;
		}
		break;
	}
	case IAM_CAMERACONTROL:
	{
		QzCComPtr<IAMCameraControl> CameraControl;
		if (FAILED(((IBaseFilter*)mDSDeviceDescriptor->getHandle())->QueryInterface(
				IID_IAMVideoProcAmp, (void**) &CameraControl))) {
			return -1;
		}
		
		if (FAILED(CameraControl->Set(mProperty, mValue, mFlags))) {
			return -1;
		}
		break;
	}

	default:
		return -1;
	}

	return 0;
}

int DS_Control::setValue(int value)
{
	int old = mValue;
	mValue = value;
	int res = update();

	if(res == -1)
		mValue = old;

	return res;
}

int DS_Control::getValue() const
{
	return mValue;
}

int DS_Control::retrieveValue()
{
	long flags;
	switch (mDShowInterfaceType)
	{
		case IAM_VIDEOPROCAMP:
		{
			QzCComPtr<IAMVideoProcAmp> VideoProcAmp;
			if (FAILED(((IBaseFilter*)mDSDeviceDescriptor->getHandle())->QueryInterface(
					IID_IAMVideoProcAmp, (void**) &VideoProcAmp))) {
				return -1;
			}
			
			if (FAILED(VideoProcAmp->Get(mProperty, (long*) &mValue, &flags))) {
				return -1;
			}
			break;
		}
		case IAM_CAMERACONTROL:
		{
			QzCComPtr<IAMCameraControl> CameraControl;
			if (FAILED(((IBaseFilter*)mDSDeviceDescriptor->getHandle())->QueryInterface(
					IID_IAMVideoProcAmp, (void**) &CameraControl))) {
				return -1;
			}
			
			if (FAILED(CameraControl->Get(mProperty, (long*) &mValue, &flags))) {
				return -1;
			}
			break;
		}
	}

	return mValue;
}

int DS_Control::reset()
{
	mValue = mDefaultValue;
	return update();
}
