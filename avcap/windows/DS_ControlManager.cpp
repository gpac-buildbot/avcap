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

#include "DS_ControlManager.h"
#include "DS_DeviceDescriptor.h"
#include "DS_Control.h"
#include "HelpFunc.h"

using namespace avcap;

// Construction & Destruction

DS_ControlManager::DS_ControlManager(DS_DeviceDescriptor *dd) :
	ControlManager(dd), mDSDeviceDescriptor(dd)
{
}

DS_ControlManager::~DS_ControlManager()
{
}

void DS_ControlManager::query()
{
	// Query for available controls.
	assert(mDSDeviceDescriptor != 0);

	// For more information - see "DS_ControlManager.h" 
	// Query the IAMVideoProcAmp COM-Interface - only supported on WDM capture devices
	QzCComPtr<IAMVideoProcAmp> VideoProcAmp;
	if (FAILED(((IBaseFilter*)mDSDeviceDescriptor->getHandle())->QueryInterface(IID_IAMVideoProcAmp,
			(void**) &VideoProcAmp))) {
		return;
	}

	int Index=0;
	int PropertyCount=10; // Number of elements in the "switch(Property)" section -> see below
	for (int Property=0; Property<PropertyCount; Property++) {
		DS_Control::Query_Ctrl Ctrl;

		if (SUCCEEDED(VideoProcAmp->GetRange(Property, &Ctrl.minimum,
				&Ctrl.maximum, &Ctrl.step, &Ctrl.default_value, &Ctrl.flags))) {
			Ctrl.id=Index;
			Ctrl.DShowInterfaceType=DS_Control::IAM_VIDEOPROCAMP;
			Ctrl.property=Property;
		} else {
			continue;
		}

		switch (Property)
		{
		case VideoProcAmp_Brightness:
			Ctrl.name = "Brightness";
			Ctrl.type = DS_Control::CTRL_TYPE_INTEGER;
			break;

		case VideoProcAmp_Contrast:
			Ctrl.name = "Contrast";
			Ctrl.type = DS_Control::CTRL_TYPE_INTEGER;
			break;
		case VideoProcAmp_Hue:
			Ctrl.name = "Hue";
			Ctrl.type = DS_Control::CTRL_TYPE_INTEGER;
			break;

		case VideoProcAmp_Saturation:
			Ctrl.name = "Saturation";
			Ctrl.type = DS_Control::CTRL_TYPE_INTEGER;
			break;

		case VideoProcAmp_Sharpness:
			Ctrl.name = "Sharpness";
			Ctrl.type = DS_Control::CTRL_TYPE_INTEGER;
			break;

		case VideoProcAmp_Gamma:
			Ctrl.name = "Gamma";
			Ctrl.type = DS_Control::CTRL_TYPE_INTEGER;
			break;

		case VideoProcAmp_ColorEnable:
			Ctrl.name = "Color Enable";
			Ctrl.type = DS_Control::CTRL_TYPE_BOOLEAN;
			break;

		case VideoProcAmp_WhiteBalance:
			Ctrl.name = "White Balance";
			Ctrl.type = DS_Control::CTRL_TYPE_INTEGER;
			break;

		case VideoProcAmp_BacklightCompensation:
			Ctrl.name = "Blacklight Compensation";
			Ctrl.type = DS_Control::CTRL_TYPE_BOOLEAN;
			break;

		case VideoProcAmp_Gain:
			Ctrl.name = "Gain";
			Ctrl.type = DS_Control::CTRL_TYPE_INTEGER;
			break;

		default:
			continue;
		}

		// Create control and add it to the list
		Control *c = 0;
		if (Ctrl.type==DS_Control::CTRL_TYPE_INTEGER) {
			c = new DS_IntControl(mDSDeviceDescriptor, &Ctrl);
		}
		
		if (Ctrl.type==DS_Control::CTRL_TYPE_BOOLEAN) {
			c = new DS_BoolControl(mDSDeviceDescriptor, &Ctrl);
		}
		
		if (c!=NULL) {
			mControls.push_back(c); // Store it in the list
			Index++;
		}
	}

	// Query the IAMCameraControl COM-Interface - only supported on WDM capture devices
	QzCComPtr<IAMCameraControl> CamControl;
	if (FAILED(((IBaseFilter*)mDSDeviceDescriptor->getHandle())->QueryInterface(IID_IAMCameraControl,
			(void**) &CamControl))) {
		return;
	}

	for (int Property=0; Property<7; Property++) {
		DS_Control::Query_Ctrl Ctrl;

		if (SUCCEEDED(CamControl->GetRange(Property, &Ctrl.minimum,
				&Ctrl.maximum, &Ctrl.step, &Ctrl.default_value, &Ctrl.flags))) {
			Ctrl.id=Index;
			Ctrl.DShowInterfaceType=DS_Control::IAM_CAMERACONTROL;
			Ctrl.property=Property;
		} else {
			continue;
		}

		switch (Property)
		{
		case CameraControl_Pan:
			Ctrl.name = "Camera Pan";
			Ctrl.type=DS_Control::CTRL_TYPE_INTEGER;
			break;
		case CameraControl_Tilt:
			Ctrl.name = "Camera Tilt";
			Ctrl.type=DS_Control::CTRL_TYPE_INTEGER;
			break;
		case CameraControl_Roll:
			Ctrl.name = "Camera Roll";
			Ctrl.type=DS_Control::CTRL_TYPE_INTEGER;
			break;
		case CameraControl_Zoom:
			Ctrl.name = "Camera Zoom";
			Ctrl.type=DS_Control::CTRL_TYPE_INTEGER;
			break;
		case CameraControl_Exposure:
			Ctrl.name = "Camera Exposure";
			Ctrl.type=DS_Control::CTRL_TYPE_INTEGER;
			break;
		case CameraControl_Iris:
			Ctrl.name = "Camera Iris";
			Ctrl.type=DS_Control::CTRL_TYPE_INTEGER;
			break;
		case CameraControl_Focus:
			Ctrl.name = "Camera Focus";
			Ctrl.type=DS_Control::CTRL_TYPE_INTEGER;
			break;

		default:
			continue;
		}

		// Create control and add it to the list
		Control *c = 0;
		if (Ctrl.type==DS_Control::CTRL_TYPE_INTEGER) {
			c = new DS_IntControl(mDSDeviceDescriptor, &Ctrl);
		}
		
		if (c!=NULL) {
			mControls.push_back(c); // Store it in the list
			Index++;
		}
	}
}

// Return the control with the specified name.

Control* DS_ControlManager::getControl(const std::string& name)
{
	// Iterate the list and find the control
	for (ListType::iterator it = mControls.begin(); it != mControls.end(); it++)
		if ((*it)->getName() == name)
			return *it;

	// no control found	
	return NULL;
}

// Return the control with the unique id.

Control* DS_ControlManager::getControl(int id)
{
	// Iterate the list and find the control
	for (ListType::iterator it = mControls.begin(); it != mControls.end(); it++)
		if ((*it)->getId() == id)
			return *it;

	// no control found
	return NULL;
}
