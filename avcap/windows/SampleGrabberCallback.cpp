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

#include <DShow.h>
#include <RefTime.h>
#include <MType.h>
#include <uuids.h>
#include <unknwn.h>
#include <qedit.h>
#include <string.h>

#include "DS_VidCapManager.h"
#include "SampleGrabberCallback.h"
#include "CaptureHandler.h"

using namespace avcap;

SampleGrabberCallback::SampleGrabberCallback(HANDLE& lock) :
	mVidCapMngr(0), mSampleGrabberFilter(0), mLock(lock)
{
}

SampleGrabberCallback::~SampleGrabberCallback()
{
	if (mSampleGrabberFilter!=0) {
		mSampleGrabberFilter->Release();
	}
}

void SampleGrabberCallback::SetVideoCaptureManager(
		DS_VidCapManager *vidCapManager)
{
	if (vidCapManager==0) {
		return;
	}
	
	mVidCapMngr=vidCapManager;
}

void SampleGrabberCallback::SetSampleGrabberFilter(
		ISampleGrabber *SampleGrabberFilter)
{
	if (SampleGrabberFilter==0) {
		return;
	}
	
	mSampleGrabberFilter=SampleGrabberFilter;
	mSampleGrabberFilter->AddRef();
}

STDMETHODIMP SampleGrabberCallback::QueryInterface(REFIID riid,
		void **ppvObject)
{
	if (0 == ppvObject)
		return E_POINTER;
	
	if (riid == IID_IUnknown /*__uuidof(IUnknown)*/) {
		*ppvObject = static_cast<IUnknown*>(this);
		return S_OK;
	}
	
	if (riid == IID_ISampleGrabberCB /*__uuidof(ISampleGrabberCB)*/) {
		*ppvObject = static_cast<ISampleGrabberCB*>(this);
		return S_OK;
	}
	
	return E_NOTIMPL;
}

STDMETHODIMP SampleGrabberCallback::SampleCB(double Time, IMediaSample *pSample)
{
	MutexGuard guard(mLock);
	
	// Call the capture handler if one is registered
	if (mVidCapMngr->getCaptureHandler() != 0) {
		BYTE *SampleBuffer=0;
	
		if (FAILED(pSample->GetPointer(&SampleBuffer))) {
			return E_FAIL;
		}
		mVidCapMngr->mSequence++;
		
		BYTE* buf = new BYTE[pSample->GetActualDataLength()];
		memcpy(buf, SampleBuffer, pSample->GetActualDataLength());

		IOBuffer* Buffer = new IOBuffer(mVidCapMngr, buf, pSample->GetActualDataLength());
		CRefTime SampleStart, SampleEnd;
		
		if (FAILED(pSample->GetTime(&SampleStart.m_time, &SampleEnd.m_time))) {
			return E_FAIL;
		}
		
		timeval SampleStartTime;
		SampleStartTime.tv_sec=(long) (SampleStart.m_time/10000000);
		SampleStartTime.tv_usec=(long)(SampleStart.m_time%10000000)/10;

		Buffer->setParams(pSample->GetActualDataLength(), IOBuffer::STATE_USED,
				SampleStartTime, mVidCapMngr->mSequence);

		if (mSampleGrabberFilter==0) {
			return E_FAIL;
		}

		AM_MEDIA_TYPE MediaType;
		ZeroMemory(&MediaType, sizeof(AM_MEDIA_TYPE));
		
		if (FAILED(mSampleGrabberFilter->GetConnectedMediaType(&MediaType))) {
			FreeMediaType(MediaType);
			return E_FAIL;
		}

		mVidCapMngr->getCaptureHandler()->handleCaptureEvent(Buffer);
		FreeMediaType(MediaType);
	}

	return S_OK;
}

STDMETHODIMP SampleGrabberCallback::BufferCB(double Time, BYTE *pBuffer,
		long BufferLen)
{
	return E_NOTIMPL;
}
