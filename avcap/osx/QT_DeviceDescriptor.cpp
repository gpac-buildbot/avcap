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

#include <stdlib.h>
#include <sstream>

#include "QT_DeviceDescriptor.h"
#include "QT_Device.h"
#include "log.h"

using namespace avcap;

// Construction & Destruction

QT_DeviceDescriptor::QT_DeviceDescriptor():
	mGrabber(0),
	mChannel(0),
	mDigitizer(0),
	mDeviceID(-1),
	mInputID(-1),
	mValid(false),
	mDevice(0)
{
}

QT_DeviceDescriptor::QT_DeviceDescriptor(int device, int input, const std::string& dev_name, const std::string& driver_name,
			SeqGrabComponent current_grabber, SGChannel current_channel):
	mGrabber(0),
	mChannel(0),
	mDigitizer(0),
	mDeviceID(device),
	mInputID(input),
	mValid(false),
	mName(dev_name),
	mDriver(driver_name),
	mDevice(0)
{
	queryCapabilities(current_grabber, current_channel);
}

QT_DeviceDescriptor::~QT_DeviceDescriptor()
{
	close();
}

CaptureDevice* QT_DeviceDescriptor::getDevice()
{
	if(!mDevice)
		mDevice = new QT_Device(this);

	return mDevice;
}


int QT_DeviceDescriptor::open()
{
	// Open the device

	if(mValid == true)
		return 0;

	mValid = false;

	// get the default SequenceGrabber-Component
	if(!(mGrabber = OpenDefaultComponent(SeqGrabComponentType, 0))) {
		logDebug("QT_DeviceDescriptor: Couldn't open SequenceGrabber Component.");
		return -1;
	}

	// and initialize the grabber
	OSErr err = SGInitialize(mGrabber);
	if(err != noErr ) {
		logDebug("QT_DeviceDescriptor: SGInitialize failed ", err);
		close();
		return -1;
	}

	// we just want to capture and not make a movie
	if((err = SGSetDataRef(mGrabber, 0, 0, seqGrabDontMakeMovie|seqGrabToMemory)) != noErr)	{
		logDebug("QT_DeviceDescriptor: SGSetDataRef failed ", err);
		close();
		return -1;
	}

	// open a channel to media-type video
	if ((err = SGNewChannel(mGrabber, VideoMediaType, &mChannel)))	{
		logDebug("QT_DeviceDescriptor: SGNewChannel failed", err);
		close();
		return -1;
	}

	// a precise device is given then open and initialize it, do nothing otherwise.
	if(mDeviceID != -1 && mInputID != -1) {
		// set the given device for the video-channel
		if((err = SGSetChannelDevice(mChannel, (unsigned char*) mDriver.c_str()))) {
			logDebug("QT_DeviceDescriptor: SGSetChannelDevice failed", err);
			return -1;
		}

		// and set the specified input
		if((err = SGSetChannelDeviceInput(mChannel, mInputID))) {
			logDebug("QT_DeviceDescriptor: SGSetChannelDeviceInput failed", err);
			return -1;
		}

		// get the digitizer associated with the device and the digitizer-info
		mDigitizer = SGGetVideoDigitizerComponent(mChannel);
		if(mDigitizer && (err = VDGetDigitizerInfo(mDigitizer, &mDigiInfo))) {
			logDebug("QT_DeviceDescriptor: VDGetDigitizerInfo failes", err);
			return -1;
		}
	}

	mValid = true;
	return 0;
}

int QT_DeviceDescriptor::close()
{
	// free all ressources and close the grabber
	if(mChannel)
		SGDisposeChannel(mGrabber, mChannel);

	if(mGrabber)
		CloseComponent(mGrabber);

	mChannel = mGrabber = 0;
	mValid = false;
	delete mDevice;
	mDevice = 0;

	return  0;
}

bool QT_DeviceDescriptor::queryCapabilities(SeqGrabComponent current_grabber, SGChannel current_channel)
{
	return true;
}

const std::string& QT_DeviceDescriptor::getName() const
{
	return mName;
}

const std::string& QT_DeviceDescriptor::getDriver() const
{
	return mDriver;
}

bool QT_DeviceDescriptor::isVideoCaptureDev() const
{
	return true;
}
