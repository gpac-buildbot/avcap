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
#include <math.h>

#include "DS_VidCapManager.h"
#include "DS_DeviceDescriptor.h"
#include "DS_FormatManager.h"
#include "IOBuffer.h"
#include "CaptureHandler.h"
#include "SampleGrabberCallback.h"

#include "HelpFunc.h"

using namespace avcap;

// Construction & Destruction

DS_VidCapManager::DS_VidCapManager(DS_DeviceDescriptor* dd,
		DS_FormatManager *fmt_mgr, int nbufs) :
	mDSDeviceDescriptor(dd), mSequence(0), mNumBuffers(nbufs)
{
	mLock = CreateMutex(NULL, FALSE, NULL);	
	mGrabberCallback = new SampleGrabberCallback(mLock);
}

DS_VidCapManager::~DS_VidCapManager()
{	
	stopCapture();
	delete mGrabberCallback;
	CloseHandle(mLock);
}

int DS_VidCapManager::init()
{
	return 0;
}

int DS_VidCapManager::destroy()
{
	return 0;
}


int DS_VidCapManager::startCapture()
{
	MutexGuard guard(mLock);

	// Start capturing
	
	mSequence=0;

	// Get the IGraphBuilder COM-Interface from capture filter
	QzCComPtr<IGraphBuilder> FilterGraph;
	if (!GetFilterGraphFromFilter(
			((IBaseFilter*)mDSDeviceDescriptor->getHandle()), &FilterGraph)) {
		return false;
	}

	// Find the video samplegrabber filter in the filter graph
	QzCComPtr<IBaseFilter> VidSampleGrabberFilter;
	QzCComPtr<ISampleGrabber> VidSampleGrabber;
	if (FAILED(FilterGraph->FindFilterByName(VIDEO_SAMPLEGRABBER_FILTER_NAME,
			&VidSampleGrabberFilter))) {
		return -1;
	}

	if (FAILED(VidSampleGrabberFilter->QueryInterface(IID_ISampleGrabber,
			(void**)&VidSampleGrabber))) {
		return -1;
	}

	VidSampleGrabber->SetCallback((ISampleGrabberCB*)mGrabberCallback, 0); // 0 means that the samplegrabber filter calls the SampleCB method
	mGrabberCallback->SetVideoCaptureManager(this);
	mGrabberCallback->SetSampleGrabberFilter(VidSampleGrabber);
	VidSampleGrabber->SetBufferSamples(TRUE);
	QzCComPtr<IMediaControl> Control;
	
	if (FAILED(FilterGraph->QueryInterface(IID_IMediaControl, (void **)&Control))) {
		return -1;
	}
	
	OAFilterState FilterState=0;
	Control->Stop();
	Control->GetState(2000, &FilterState);
	
	if (FAILED(Control->Run())) {
		return -1;
	}
	FilterState=0;
	
	if (FAILED(Control->GetState(2000, &FilterState))) {
		return -1;
	}
	
	if (FilterState!=State_Running) {
		return -1;
	}

	return 0;
}

int DS_VidCapManager::stopCapture()
{
	{
		MutexGuard guard(mLock);
		mGrabberCallback->SetVideoCaptureManager(0);
	}
	
	// Get the IGraphBuilder COM-Interface from capture filter
	QzCComPtr<IGraphBuilder> FilterGraph;
	QzCComPtr<ICaptureGraphBuilder2> CaptureGraphBuilder;
	
	if (!GetFilterGraphFromFilter(
			((IBaseFilter*)mDSDeviceDescriptor->getHandle()), &FilterGraph)) {
		return -1;
	}

	QzCComPtr<IMediaControl> Control;
	
	if (FAILED(FilterGraph->QueryInterface(IID_IMediaControl, (void **)&Control))) {
		return -1;
	}
	
	if (FAILED(Control->Stop())) {
		return -1;
	}
	
	// Find the video samplegrabber filter in the filter graph
	QzCComPtr<IBaseFilter> VidSampleGrabberFilter;
	QzCComPtr<ISampleGrabber> VidSampleGrabber;
	if (FAILED(FilterGraph->FindFilterByName(VIDEO_SAMPLEGRABBER_FILTER_NAME,
			&VidSampleGrabberFilter))) {
		return -1;
	}

	if (FAILED(VidSampleGrabberFilter->QueryInterface(IID_ISampleGrabber,
			(void**)&VidSampleGrabber))) {
		return -1;
	}

	VidSampleGrabber->SetCallback((ISampleGrabberCB*)0, 0); // 0 means that the samplegrabber filter calls the SampleCB method

	OAFilterState FilterState=0;	
	if (FAILED(Control->GetState(2000, &FilterState))) {
		return -1;
	}

	if (FilterState!=State_Stopped) {
		return -1;
	}
	
	return 0;
}

int DS_VidCapManager::enqueue(IOBuffer *io_buf)
{
	delete[] ((BYTE*) io_buf->getPtr());
	delete io_buf;

	return 0;
}

IOBuffer* DS_VidCapManager::dequeue()
{
	return 0;
}

int DS_VidCapManager::getNumIOBuffers()
{
	return mNumBuffers;
}
