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
#include <iostream>
#include <sys/ioctl.h>

#include "V4L2_Tuner.h"
#include "V4L2_DeviceDescriptor.h"

#ifdef AVCAP_HAVE_V4L2
#include <linux/videodev2.h>
#else
#include <linux/videodev.h>
#endif

using namespace avcap;

// Construction & Destruction

V4L2_Tuner::V4L2_Tuner(V4L2_DeviceDescriptor *dd, int index, const std::string &name, int type, 
			 int caps, uint high, uint low)
	:mDeviceDescriptor(dd), mIndex(index),mName(name), mType(type), mCapabilities(caps), mRangeHigh(high), 
	mRangeLow(low) 
{
	// set the step width
	if(caps & TUNER_CAP_LOW)
		mStep = 62.5;
	else
		mStep = 62500.0;
}

V4L2_Tuner::~V4L2_Tuner()
{
}

int V4L2_Tuner::setMono()
{
	// Activate audio mode mono.
	return setAudioMode(V4L2_TUNER_MODE_MONO);
}

int V4L2_Tuner::setStereo()
{
	// Activate audio mode stereo.
	return setAudioMode(V4L2_TUNER_MODE_STEREO);
}

int V4L2_Tuner::setLang1()
{
	// Activate audio mode language 1.
	return setAudioMode(V4L2_TUNER_MODE_LANG1);
}

int V4L2_Tuner::setLang2()
{
	// Activate audio mode language 2.
	return setAudioMode(V4L2_TUNER_MODE_LANG2);
}

int V4L2_Tuner::setSAP()
{
	// Activate audio mode secondary audio program (SAP).
	return setAudioMode(V4L2_TUNER_MODE_SAP);
}

int V4L2_Tuner::setAudioMode(int mode)
{
	// Actualy sets the audio mode.
	// other modes than mono and stereo don't apply to radio V4L2_Tuners 
	if (isRadioTuner())
	{
		if (mode != V4L2_TUNER_MODE_MONO && mode != V4L2_TUNER_MODE_STEREO)
			return -1;
	}

	// test, if requested mode is supported by the driver
	if(mode == V4L2_TUNER_MODE_STEREO && (!(mCapabilities & V4L2_TUNER_CAP_STEREO)))
		return -1;
		
	if(mode == V4L2_TUNER_MODE_LANG1 && (!(mCapabilities & V4L2_TUNER_CAP_LANG1)))
		return -1;

	if(mode == V4L2_TUNER_MODE_LANG2 && (!(mCapabilities & V4L2_TUNER_CAP_LANG2)))
		return -1;

	if(mode == V4L2_TUNER_MODE_SAP && (!(mCapabilities & V4L2_TUNER_CAP_SAP)))
		return -1;

	struct v4l2_tuner tuner;
	memset (&tuner, 0, sizeof(v4l2_tuner));
	
	tuner.index = mIndex;
	tuner.audmode = mode;

	// and set the new mode
	return ioctl(mDeviceDescriptor->getHandle(), VIDIOC_S_TUNER, &tuner);
}

int V4L2_Tuner::setFreq(double f)
{
	// Set the frequency (in MHz).
	struct v4l2_frequency	freq;
	memset(&freq, 0, sizeof(v4l2_frequency));
	freq.tuner = mIndex;

	// frequency is given in units of some frequency step
	freq.frequency=(int)((f/mStep)+0.5);
	
	// check bounds
	if(freq.frequency > mRangeHigh && (mRangeHigh != 0xffffffffUL))
		freq.frequency = mRangeHigh;
	if(freq.frequency < mRangeLow)
		freq.frequency = mRangeLow;

	// and set the value
	return ioctl(mDeviceDescriptor->getHandle(), VIDIOC_S_FREQUENCY, &freq);
}

double V4L2_Tuner::getFreq() const
{
	// Return the current frequency (in MHz).
	struct v4l2_frequency	freq;
	memset(&freq, 0, sizeof(v4l2_frequency));
	freq.tuner = mIndex;
	
	// get the frequncy
	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOC_G_FREQUENCY, &freq))
		return -1;
	
	// and multiply it with the step width to obtain MHz
	return freq.frequency * mStep;
}

int V4L2_Tuner::increaseFreq()
{
	// Increase the frequency one unit step.

	double freq;
	
	if((freq = getFreq()) < 0.0)
		return -1;
		
	return setFreq(freq+mStep);
}

int V4L2_Tuner::decreaseFreq()
{
	// Decrease the frequency one unit step.

	double freq;
	
	if((freq = getFreq()) < 0.0)
		return -1;
		
	return setFreq(freq-mStep);
}

int V4L2_Tuner::getSignalStrength() const
{
	// Return the strength of the current signal, if available.

	struct v4l2_tuner	tuner;
	memset(&tuner, 0, sizeof(v4l2_tuner));
	tuner.index = mIndex;
	
	// get the strength
	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOC_G_TUNER, &tuner)==-1)
		return -1;
		
	return tuner.signal;
}

int V4L2_Tuner::getAFCValue() const
{
	// Return the automatic frequency control value

	struct v4l2_tuner	tuner;
	memset(&tuner, 0, sizeof(V4L2_Tuner));
	tuner.index = mIndex;
	
	// get the afc value
	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOC_G_TUNER, &tuner)==-1)
		return 0;
		
	return tuner.afc;
}

int V4L2_Tuner::finetune(int maxsteps)
{
	// Fine-tunes the frequency by means of the afc-value with maximal maxsteps.

	int steps = 0, afc, res;
	
	// do we have still steps and an afc != 0
	while (steps++ < maxsteps && (afc = getAFCValue()) != 0) {
		// then adjust the frequency
		if(afc < 0)
			res = increaseFreq();
		else
			res = decreaseFreq();
		
		if(res == -1)
			return -1;
	}
	
	return 0;
}


