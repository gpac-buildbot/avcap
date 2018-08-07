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
#include <string.h>

#include "DS_FormatManager.h"
#include "DS_DeviceDescriptor.h"

#include "HelpFunc.h"
#include "FormatNames.h"
#include "fourcc.h" 
#include "Dvdmedia.h" 

using namespace avcap;

// Construction & Destruction

DS_FormatManager::DS_FormatManager(DS_DeviceDescriptor *dd) :
	FormatManager(dd), mDSDeviceDescriptor(dd), mFrameRate(0)
{
}

DS_FormatManager::~DS_FormatManager()
{
	// Delete the format objects
	for (ListType::iterator it = mFormats.begin(); it != mFormats.end(); it++) {
		DeleteMediaType((AM_MEDIA_TYPE*)(*it)->getMediaType());
	}

	if (mCurrentFormat!=NULL) {
		DeleteMediaType((AM_MEDIA_TYPE*)mCurrentFormat);
	}
}

void DS_FormatManager::query()
{
	// Find available formats and video standards.

	// Query the video standards first
	queryVideoStandards();

	/* DirectShow documentation says: "In a WDM device, 
	 * the IAMStreamConfig interface is used to report
	 * which formats the device supports, and to set the format. 
	 * (For legacy VFW devices, use the VfW driver implemented dialog"
	 * Enumerate the video formats of the currently selected output pin 
	 * with the IAMStreamConfig Interface which has to be 
	 * exposed on the output pin of a WDM capture filter. */

	QzCComPtr<IPin> VideoOutPin;
	if (!getCurrentConnectedVideoPin(
			((IBaseFilter*)mDSDeviceDescriptor->getHandle()), &VideoOutPin)) {
		return;
	}
	
	// Output pin was found - query for the IAMConfigStream interface on this output pin
	QzCComPtr<IAMStreamConfig> StreamConfig;
	if (FAILED(VideoOutPin->QueryInterface(IID_IAMStreamConfig,
			(void**)&StreamConfig))) {
		return;
	}

	// Enumerate all mediatypes 
	// The same mediatypes are combined to one.
	int Count = 0, Size = 0;
	if (FAILED(StreamConfig->GetNumberOfCapabilities(&Count, &Size))) {
		return;
	}
	
	if (Size!=sizeof(VIDEO_STREAM_CONFIG_CAPS)) {
		return;
	}
	
	for (int Format = 0; Format < Count; Format++) {
		VIDEO_STREAM_CONFIG_CAPS scc;
		AM_MEDIA_TYPE *MediaType;
	
		if (SUCCEEDED(StreamConfig->GetStreamCaps(Format, &MediaType,
				(BYTE*)&scc))) {
		
			// Look if we have a similarly MediaType
			avcap::Format *fmt = 0;
			for (ListType::iterator Iter = mFormats.begin(); Iter
					!= mFormats.end(); Iter++) {
				if (((AM_MEDIA_TYPE*)(*Iter)->getMediaType())->majortype==MediaType->majortype && ((AM_MEDIA_TYPE*)(*Iter)->getMediaType())->subtype==MediaType->subtype) {
					fmt = *Iter;
				}
			}

			bool created = false;
			if (!fmt) {
				FOURCCMap FourCC(&MediaType->subtype);
				std::string fmt_name;
				GetVideoFormatName(&MediaType->subtype, fmt_name);
				
				// correct fourcc for RGB24 
				if(*(&MediaType->subtype) == MEDIASUBTYPE_RGB24) {
					FourCC.SetFOURCC(PIX_FMT_RGB24);
				}
				
				if(FourCC.GetFOURCC() == PIX_FMT_I420) {
					fmt_name = "YU12";
					FourCC.SetFOURCC(PIX_FMT_YUV420);
				}
				
				fmt = new avcap::Format(fmt_name, FourCC.GetFOURCC());
				fmt->setMediaType(MediaType);
				created = true;
			}

			VIDEOINFOHEADER VideoInfo;
			// We have to call StreamConfig->SetFormat(MediaType) once before calling the setFormat(Format *fmt) - it's strange but 
			// if we don't do this step the method setFormat(...) returns only false.
			if ((!getVideoInfoHeader((AM_MEDIA_TYPE*)MediaType, &VideoInfo))
					|| (StreamConfig->SetFormat(MediaType)!=S_OK)) {

				if (MediaType!=NULL) {
					DeleteMediaType(MediaType);
				}
				
				if (created)
					delete fmt;
				
				continue;
			}

			fmt->addResolution(VideoInfo.bmiHeader.biWidth,
					VideoInfo.bmiHeader.biHeight);

			// Store it in the format list
			if (created)
				mFormats.push_back(fmt);
		}
	}

	// synchronize the general format parameters with the driver settings
	getParams();
}

int DS_FormatManager::setFormat(Format* f)
{
	if (f == 0)
		return -1;

	// On Win32, formats are identified only by the AM_MEDIA_TYPE DirectShow structure
	CopyMediaType((AM_MEDIA_TYPE*)mCurrentFormat,
			(AM_MEDIA_TYPE*)f->getMediaType());
	mModified=true;

	// Try to set the new format
	if (flush()<0) {
		mModified=false;
		getParams();
		return -1;
	}

	return 0;
}

int DS_FormatManager::getParams()
{
	// Read out the current parameters from the driver

	if (mCurrentFormat!=NULL) {
		DeleteMediaType((AM_MEDIA_TYPE*)mCurrentFormat);
		mCurrentFormat=NULL;
	}

	QzCComPtr<IPin> VideoOutPin;
	if (!getCurrentConnectedVideoPin(
			((IBaseFilter*)mDSDeviceDescriptor->getHandle()), &VideoOutPin)) {
		return -1;
	}

	// Output pin was found - query for the IAMStreamConfig interface on this output pin
	QzCComPtr<IAMStreamConfig> StreamConfig;
	if (FAILED(VideoOutPin->QueryInterface(IID_IAMStreamConfig,
			(void**)&StreamConfig))) {
		return -1;
	}

	AM_MEDIA_TYPE *MediaType=NULL;
	if (FAILED(StreamConfig->GetFormat(&MediaType))) {
		if (MediaType!=NULL) {
			DeleteMediaType(MediaType);
		}
		return -1;
	}

	VIDEOINFOHEADER VideoInfo;
	if (!getVideoInfoHeader(MediaType, &VideoInfo)) {
		if (MediaType!=NULL) {
			DeleteMediaType(MediaType);
		}
		return -1;
	}

	mWidth = VideoInfo.bmiHeader.biWidth;
	mHeight = VideoInfo.bmiHeader.biHeight;
	mBytesPerLine = VideoInfo.bmiHeader.biWidth*(VideoInfo.bmiHeader.biBitCount/8);

	mFrameRate = (int) ((LONGLONG)(10000000 / VideoInfo.AvgTimePerFrame )); 

	mCurrentFormat = MediaType;
	if (VideoInfo.bmiHeader.biSizeImage<=0) {
		mImageSize = mWidth*mHeight*(VideoInfo.bmiHeader.biBitCount/8);
	} else {
		mImageSize = VideoInfo.bmiHeader.biSizeImage;
	}

	return 0;
}

Format* DS_FormatManager::getFormat()
{
	if (mCurrentFormat==NULL) {
		return NULL;
	}
	Format *res = 0;

	if (getParams()<0) {
		return NULL;
	}

	// Find the corresponding format object
	for (ListType::iterator it = mFormats.begin(); it != mFormats.end(); it++) {
		if (((AM_MEDIA_TYPE*)(*it)->getMediaType())->majortype == ((AM_MEDIA_TYPE*)mCurrentFormat)->majortype && ((AM_MEDIA_TYPE*)(*it)->getMediaType())->subtype == ((AM_MEDIA_TYPE*)mCurrentFormat)->subtype) {
			res = *it;
			break;
		}
	}

	return res;
}

// Set the image width.

int DS_FormatManager::setResolution(int w, int h)
{
	mWidth = w;
	mHeight = h;
	mModified = true;

	if (flush()<0) {
		mModified=false;
		getParams();
	}

	return 0;
}

int DS_FormatManager::getWidth()
{
	if (getParams()<0) {
		return 0;
	}
	return mWidth;
}

int DS_FormatManager::getHeight()
{
	if (getParams()<0) {
		return 0;
	}
	return mHeight;
}

int DS_FormatManager::getBytesPerLine()
{
	if (getParams()<0) {
		return 0;
	}

	return mBytesPerLine;
}

size_t DS_FormatManager::getImageSize()
{
	if (getParams()<0) {
		return 0;
	}

	return mImageSize;
}

int DS_FormatManager::setFramerate(int fps)
{ 
	int old_fr = mFrameRate;
	mFrameRate = fps; 
	if(flush() < 0)
		mFrameRate = old_fr;

	return mFrameRate == fps ? 0 : -1; 
}

int DS_FormatManager::getFramerate()
{
	if(mFrameRate == 0)
		getParams();

	return mFrameRate;
}

// Flush settings. Forward all modifications to the driver.

int DS_FormatManager::flush()
{
	// Flush settings. Forward all modifications to the driver.
	// if something changed
	if (mModified) {
		QzCComPtr<IPin> VideoOutPin;
		if (!getCurrentConnectedVideoPin(
				((IBaseFilter*)mDSDeviceDescriptor->getHandle()), &VideoOutPin)) {
			return -1;
		}
		
		// Output pin was found - query for the IAMConfigStream interface on this ouput pin
		QzCComPtr<IAMStreamConfig> StreamConfig;
		if (FAILED(VideoOutPin->QueryInterface(IID_IAMStreamConfig,
				(void**)&StreamConfig))) {
			return -1;
		}

		if (((AM_MEDIA_TYPE*)mCurrentFormat)->formattype
				== FORMAT_VideoInfo && ((AM_MEDIA_TYPE*)mCurrentFormat)->cbFormat
				>= sizeof(VIDEOINFOHEADER)) {
			VIDEOINFOHEADER *Vih = reinterpret_cast<VIDEOINFOHEADER*>(((AM_MEDIA_TYPE*)mCurrentFormat)->pbFormat);
			Vih->bmiHeader.biHeight=mHeight;
			Vih->bmiHeader.biWidth=mWidth;
			if(mFrameRate != 0)
				Vih->AvgTimePerFrame =(LONGLONG)(10000000 / mFrameRate); 
		}

		if (((AM_MEDIA_TYPE*)mCurrentFormat)->formattype
				== FORMAT_VideoInfo2 && ((AM_MEDIA_TYPE*)mCurrentFormat)->cbFormat
				>= sizeof(VIDEOINFOHEADER2)) {
			VIDEOINFOHEADER2 *Vih = reinterpret_cast<VIDEOINFOHEADER2*>(((AM_MEDIA_TYPE*)mCurrentFormat)->pbFormat);
			Vih->bmiHeader.biHeight=mHeight;
			Vih->bmiHeader.biWidth=mWidth;
			if(mFrameRate != 0)
				Vih->AvgTimePerFrame =(LONGLONG)(10000000 / mFrameRate); 
		}

		if (((AM_MEDIA_TYPE*)mCurrentFormat)->formattype
				== FORMAT_MPEGVideo && ((AM_MEDIA_TYPE*)mCurrentFormat)->cbFormat
				>= sizeof(FORMAT_MPEGVideo)) {
			MPEG1VIDEOINFO *Mpeg1 = reinterpret_cast<MPEG1VIDEOINFO*>(((AM_MEDIA_TYPE*)mCurrentFormat)->pbFormat);
			Mpeg1->hdr.bmiHeader.biHeight=mHeight;
			Mpeg1->hdr.bmiHeader.biWidth=mWidth;
		}

		if (((AM_MEDIA_TYPE*)mCurrentFormat)->formattype
				== FORMAT_MPEG2Video && ((AM_MEDIA_TYPE*)mCurrentFormat)->cbFormat
				>= sizeof(FORMAT_MPEG2Video)) {
			MPEG2VIDEOINFO *Mpeg2 = reinterpret_cast<MPEG2VIDEOINFO*>(((AM_MEDIA_TYPE*)mCurrentFormat)->pbFormat);
			Mpeg2->hdr.bmiHeader.biHeight=mHeight;
			Mpeg2->hdr.bmiHeader.biWidth=mWidth;
		}

		HRESULT hr=StreamConfig->SetFormat((AM_MEDIA_TYPE*)mCurrentFormat);
		if (hr!=S_OK) {
			mModified = false;
			return -1;
		}

		mModified = false;

		return 0;
	} else {
		return 0;
	}
}

void DS_FormatManager::queryVideoStandards()
{
	// Query for available video standards.
	// Delete the old standards
	for (VideoStandardList::iterator it = mStandards.begin(); it
			!= mStandards.end(); it++)
		delete *it;

	// Clear the list
	mStandards.clear();

	// Get the VideoStandards from the AnalogVideoDecoder
	long VideoStandard=0;
	QzCComPtr<IAMAnalogVideoDecoder> VideoDecoder;
	if (SUCCEEDED(((IBaseFilter*)mDSDeviceDescriptor->getHandle())->QueryInterface(IID_IAMAnalogVideoDecoder,
			(void**)&VideoDecoder))) {
		if (FAILED(VideoDecoder->get_AvailableTVFormats(&VideoStandard))) {
			return;
		}
	} else {
		return;
	}

	// Get the names of the VideoStandards and store it in the list
	GetVideoStandardName(VideoStandard, mStandards);
}

const VideoStandard* DS_FormatManager::getVideoStandard()
{
	// Get the VideoStandard from the AnalogVideoDecoder
	long VideoStandard=0;
	QzCComPtr<IAMAnalogVideoDecoder> VideoDecoder;

	if (SUCCEEDED(((IBaseFilter*)mDSDeviceDescriptor->getHandle())->QueryInterface(IID_IAMAnalogVideoDecoder,
			(void**)&VideoDecoder))) {
		if (FAILED(VideoDecoder->get_TVFormat(&VideoStandard))) {
			return NULL;
		}
	} else {
		return NULL;
	}

	// Get the name of the VideoStandard and store it in the list
	std::list<avcap::VideoStandard*> vsList; // List has to store only one element
	GetVideoStandardName(VideoStandard, vsList);
	std::list<avcap::VideoStandard*>::iterator vsListIter=vsList.begin(); // Get the first element of the list

	// Find the corresponding object
	for (VideoStandardList::iterator it = mStandards.begin(); it
			!= mStandards.end(); it++) {
		if ((*it)->id == (*vsListIter)->id) {
			delete *vsListIter;
			return *it;
		}
	}

	return 0;
}

int DS_FormatManager::setVideoStandard(const VideoStandard* std)
{
	// Set the video standard
	QzCComPtr<IAMAnalogVideoDecoder> VideoDecoder;
	if (((IBaseFilter*)mDSDeviceDescriptor->getHandle())->QueryInterface(IID_IAMAnalogVideoDecoder,
			(void**)&VideoDecoder)!=S_OK) {
		return -1;
	}

	if (VideoDecoder->put_TVFormat(std->id)!=S_OK) {
		return -1;
	}

	return 0;
}

bool DS_FormatManager::getVideoInfoHeader(AM_MEDIA_TYPE *MediaType,
		VIDEOINFOHEADER *VideoInfoHeader)
{
	if (MediaType->formattype == FORMAT_VideoInfo) {

		// Check the buffer size.
		if (MediaType->cbFormat >= sizeof(VIDEOINFOHEADER)) {
			VIDEOINFOHEADER *Vih =
					reinterpret_cast<VIDEOINFOHEADER*>(MediaType->pbFormat);
			memcpy(VideoInfoHeader, Vih, sizeof(VIDEOINFOHEADER));
			return true;
		}
	}

	if (MediaType->formattype == FORMAT_VideoInfo2) {
		// Check the buffer size.
		if (MediaType->cbFormat >= sizeof(VIDEOINFOHEADER2)) {
			VIDEOINFOHEADER2 *Vih =
					reinterpret_cast<VIDEOINFOHEADER2*>(MediaType->pbFormat);
			VideoInfoHeader->AvgTimePerFrame=Vih->AvgTimePerFrame;
			memcpy(&VideoInfoHeader->bmiHeader, &Vih->bmiHeader,
					sizeof(BITMAPINFOHEADER));
		
			VideoInfoHeader->dwBitErrorRate=Vih->dwBitErrorRate;
			VideoInfoHeader->dwBitRate=Vih->dwBitRate;
			
			memcpy(&VideoInfoHeader->rcSource, &Vih->rcSource, sizeof(RECT));
			memcpy(&VideoInfoHeader->rcTarget, &Vih->rcTarget, sizeof(RECT));
			
			return true;
		}
	}

	if (MediaType->formattype == FORMAT_MPEGVideo) {
		// Check the buffer size.
		if (MediaType->cbFormat >= sizeof(FORMAT_MPEGVideo)) {
			MPEG1VIDEOINFO *Mpeg1 =
					reinterpret_cast<MPEG1VIDEOINFO*>(MediaType->pbFormat);
			memcpy(VideoInfoHeader, &Mpeg1->hdr, sizeof(VIDEOINFOHEADER));
			return true;
		}
	}

	if (MediaType->formattype == FORMAT_MPEG2Video) {
		// Check the buffer size.
		if (MediaType->cbFormat >= sizeof(FORMAT_MPEG2Video)) {
			MPEG2VIDEOINFO *Mpeg2 =
					reinterpret_cast<MPEG2VIDEOINFO*>(MediaType->pbFormat);
			VideoInfoHeader->AvgTimePerFrame=Mpeg2->hdr.AvgTimePerFrame;
			memcpy(&VideoInfoHeader->bmiHeader, &Mpeg2->hdr.bmiHeader,
					sizeof(BITMAPINFOHEADER));
			
			VideoInfoHeader->dwBitErrorRate=Mpeg2->hdr.dwBitErrorRate;
			VideoInfoHeader->dwBitRate=Mpeg2->hdr.dwBitRate;
			
			memcpy(&VideoInfoHeader->rcSource, &Mpeg2->hdr.rcSource,
					sizeof(RECT));
			memcpy(&VideoInfoHeader->rcTarget, &Mpeg2->hdr.rcTarget,
					sizeof(RECT));
			
			return true;
		}
	}

	return false;
}

bool DS_FormatManager::getCurrentConnectedVideoPin(IBaseFilter *CaptureFilter,
		IPin **VideoPin)
{
	if (CaptureFilter==NULL) {
		return false;
	}

	// Search for the first connected output pin on the capture filter
	std::list<IPin*> PinList;
	EnumPinsOnFilter(CaptureFilter, PinList);
	for (std::list<IPin*>::iterator Iter=PinList.begin(); Iter!=PinList.end(); Iter++) {
		PIN_DIRECTION PinDir;
		if (FAILED((*Iter)->QueryDirection(&PinDir))) {
			continue;
		}
		
		QzCComPtr<IPin> TempPin;
		if (PinDir==PINDIR_OUTPUT && ((*Iter)->ConnectedTo(&TempPin)==S_OK)) {
			(*Iter)->AddRef();
			*VideoPin=(*Iter);
			DeleteList(PinList);
			return true;
		}
		TempPin.Release();
	}

	DeleteList(PinList);
	return false;
}

