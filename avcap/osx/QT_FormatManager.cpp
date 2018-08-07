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
#include <stdio.h>

#include "QT_FormatManager.h"
#include "QT_DeviceDescriptor.h"
#include "DeviceDescriptor.h"
#include "ProbeValues.h"
#include "log.h"


using namespace avcap;

// Construction & Destruction

QT_FormatManager::QT_FormatManager(QT_DeviceDescriptor *dd):
	FormatManager((DeviceDescriptor*) dd),
	mQTDeviceDescriptor(dd),
	mGWorld(0)
{
	mCurrentBounds.left = mCurrentBounds.top = 0;
	mCurrentBounds.bottom = mCurrentBounds.right = 0;
}

QT_FormatManager::~QT_FormatManager()
{
}

void QT_FormatManager::query()
{
	// Find available formats and video standards.

	OSErr err;
	VDCompressionListHandle comp_list = (VDCompressionListHandle) NewHandle(0);
	
	// the compression-list contains the format-description
	if((err = VDGetCompressionTypes(mQTDeviceDescriptor->getDigitizer(), comp_list))) {
		logDebug("QT_FormatManager: VDGetCompressionTypes failed ", err);
		return;
	}

	// iterate through the compression-list
	int comp_list_len = sizeof(**comp_list) / sizeof(VDCompressionList);	
	for(int i = 0; i < comp_list_len; i++) {
		uint32_t fourcc = OSType2Fourcc(comp_list[i]->cType);
		
		// create a fourcc-string and propper name
		char fourcc_buf[20];
		snprintf(fourcc_buf, 20, "%d, %c%c%c%c", fourcc, (char)(fourcc >> 24), (char)(fourcc >> 16), (char)(fourcc >> 8), (char)(fourcc >> 0));
		char name_buf[20];
		snprintf(name_buf, 20, "%s", comp_list[i]->name);
		
		std::string msg = "examining format: ";
		msg += name_buf; 
		msg += "(";
		msg += fourcc_buf;
		msg += ")";
		logDebug(msg);
		
		// if fourcc is not one of the known types, we need explicit decompression to YUYV
		// performed by the capture-manager
		if(fourcc == 0) {
			mNeedsDecompression = true;
			fourcc = PIX_FMT_YUYV;
			strncpy(name_buf, "YUYV - format", 20);			
		} else {
			mNeedsDecompression = false;
		}

		// create format and probe setting the standard resolutions
		Format *fmt = new Format(name_buf, fourcc);
		int num_res_found = probeResolutions(fmt);
		
		// use at most one format, if a valid resolution has been found
		if(num_res_found) {
			mFormats.push_back(fmt);
			break;
		}
		else delete fmt;
	}
	
	// finaly, set the chosen format 
	if(mFormats.size()) {
		Resolution* r = mFormats.front()->getResolutionList().front();
		setResolution(r->width, r->height);
		
		if(setFormat(mFormats.front()) == -1)
			logDebug("QT_FormatManager: SetFormat failed.");
	}

	// dispose compression-list
	DisposeHandle((Handle) comp_list);
}

int QT_FormatManager::probeResolutions(Format* fmt)
{
	int found = 0;
	OSErr err;
	Rect bounds;
	
	// try to set some standard resolutions for that format
	for(int i = 0; i < NumResolutions; i++) {			
		// add to the resolution-list of the format, if it works
		if(checkResolution(Resolutions[i][0], Resolutions[i][1]) != -1) {
			fmt->addResolution(Resolutions[i][0], Resolutions[i][1]);
			found++;
#ifdef DEBUG
			std::cout<<"Found resolution: "<<Resolutions[i][0]<<"x"<<Resolutions[i][1]<<std::endl;
#endif
		}
	}
		
	return found;
}

int QT_FormatManager::setFormat(Format* f)
{
	// Set a format
	// a QT-format has at most one format, so just check, if it is the one 
	if(!f) {
		logDebug("QT_FormatManager: no format given.");
		return -1;
	}
	
	return setFormat(f->getFourcc());
}

int QT_FormatManager::setFormat(uint32_t fourcc)
{
	if(!getFormatList().size()) {
		logDebug("QT_FormatManager: format list is empty.");
		return -1;
	}
	
	if(getFormatList().front()->getFourcc() != fourcc) {
		logDebug("QT_FormatManager: invalid format.");
		return -1;
	}
		
	return 0;
}

Format* QT_FormatManager::getFormat()
{
	if(getFormatList().size() == 0)
		return 0;
		
	return getFormatList().front();
}

int QT_FormatManager::updateGWorld(const Rect* bounds)
{
	// create and set a new graphics-world object to reflect desired dimensions
	OSErr err;
	if((err = QTNewGWorld(&mGWorld, 32, bounds, 0, 0, 0))) {
		logDebug("QT_FormatManager: QTNewGWorld failed", err);		
	}
	
	if((err = SGSetGWorld(mQTDeviceDescriptor->getGrabber(), mGWorld, 0))) {
		logDebug("QT_FormatManager: SGSetGWorld failed", err);
		return -1;
	}

	return 0;
}


int QT_FormatManager::setResolution(int w, int h)
{
	if(!getFormat())
		return -1;

	// check, whether the desired resolution is valid for the format
	const Format::ResolutionList_t& resolutions = getFormat()->getResolutionList();
	bool found = false;
	for(Format::ResolutionList_t::const_iterator i = resolutions.begin(); i != resolutions.end(); i++) {
		if((*i)->width == w && (*i)->height == h) {
			found = true;
			break;
		}
	}
	
	// if not return failure
	if(!found)
		return -1;
		
	// set video-rect to the new resolution
	Rect bounds = {0, 0, h, w};
	OSErr err = SGSetVideoRect(mQTDeviceDescriptor->getChannel(), &bounds);
	
	// if that didn't work, try to adjust the channel bounds
	if(err != noErr) {
		Rect default_bounds;
		SGGetSrcVideoBounds(mQTDeviceDescriptor->getChannel(), &default_bounds);
		SGSetVideoRect(mQTDeviceDescriptor->getChannel(), &default_bounds);

		err = SGSetChannelBounds(mQTDeviceDescriptor->getChannel(), &bounds);
	} 
	
	// no way, we failed
	if(err != noErr) {
		logDebug("QT_FormatManager: setResolution failed", err);
		return -1;
	}
	
	// update the graphics-world object
	if(err == noErr || getFormatList().size() == 0)
		err = updateGWorld(&bounds);
	else
		err = -1;
	
	// and update the current bounds
	if(err == noErr) {
		mCurrentBounds.right = w;
		mCurrentBounds.bottom = h;
	}
	
	return err == noErr ? 0 : -1;
}

int QT_FormatManager::checkResolution(int w, int h)
{
	OSErr err;
	int res = -1;

	// check, whether the resolution is setable for the format or not
	Rect bounds = {0, 0, h, w};
	
	// try to set video rect or channel-bounds if that fails
	err = SGSetVideoRect(mQTDeviceDescriptor->getChannel(), &bounds);
	if(err != noErr) {
		Rect default_bounds;
		SGGetSrcVideoBounds(mQTDeviceDescriptor->getChannel(), &default_bounds);
		SGSetVideoRect(mQTDeviceDescriptor->getChannel(), &default_bounds);

		err = SGSetChannelBounds(mQTDeviceDescriptor->getChannel(), &bounds);
	} 
	
	if(err != noErr) {
		return -1;
	}
	
	// try to update the graphics world
	if(err = updateGWorld(&bounds)) {
		return -1;
	}
	
	// then try to start grabbing
	// this is necessary to get a valid image-description 
	if((err = SGSetChannelUsage(mQTDeviceDescriptor->getChannel(), seqGrabRecord))) {
		logDebug("QT_FormatManager: SGSetChannelUsage failed", err);
		return -1;
	}

	if((err = SGStartRecord(mQTDeviceDescriptor->getGrabber()))) {
		logDebug("QT_FormatManager: SGStartRecord failed", err);
		return -1;
	}

	// get image descripton
	ImageDescriptionHandle img_desc = (ImageDescriptionHandle) NewHandle(0);
	if((err = SGGetChannelSampleDescription(mQTDeviceDescriptor->getChannel(), (Handle) img_desc))) {
		logDebug("QT_FormatManager: SGGetChannelSampleDescription failed", err);
		DisposeHandle((Handle) img_desc);
		return -1;
	}
	
	// and compare actual resolution with the desired one
	SInt32 width = 0, height = 0;
	width = (**img_desc).width;
	height = (**img_desc).height;
	
	if(height == h && width == w) {
		res = 0;
	}
	
	// dispose image description and stop capture
	DisposeHandle((Handle) img_desc);
	if((err = SGStop(mQTDeviceDescriptor->getGrabber()))) {
		logDebug("QT_FormatManager: SGStop failed", err);
		return -1;
	}
		
	return res;
}


int QT_FormatManager::getWidth()
{
	return mCurrentBounds.right;
}

int QT_FormatManager::getHeight()
{
	return mCurrentBounds.bottom;
}


size_t QT_FormatManager::getImageSize()
{
	size_t res = 0;
	if(getFormat()->getFourcc() == PIX_FMT_YUYV && mNeedsDecompression) {
		// decompressed YUYV
		res = getWidth()*getHeight()*2;
	} else {
		// otherwise we determine the image-size by means of the image-description
		ImageDescriptionHandle img_desc = (ImageDescriptionHandle) NewHandle(0);
		OSErr err;

		if((err = SGGetChannelSampleDescription(mQTDeviceDescriptor->getChannel(), (Handle) img_desc))) {
			logDebug("QT_FormatManager: SGGetChannelSampleDescription failed", err);
			DisposeHandle((Handle) img_desc);
			return 0;
		}

		// get the number of bytes per row
		SInt32 rowBytes = 0;
		OSStatus status = ICMImageDescriptionGetProperty(img_desc, kQTPropertyClass_ImageDescription,
			kICMImageDescriptionPropertyID_RowBytes, sizeof(rowBytes), &rowBytes, 0);	
		if(status != noErr) {
			logDebug("QT_FormatManger: ICMImageDescriptionGetProperty failed", status);
		}
		
		DisposeHandle((Handle) img_desc);
		
		// and multiply by height 
		res = rowBytes * getHeight();
	}
	
	return res; 
}

int QT_FormatManager::setFramerate(int fps)
{
	OSErr err;
	if((err = SGSetFrameRate(mQTDeviceDescriptor->getChannel(), FloatToFixed((float) fps)))) {
		logDebug("QT_FormatManager: SGSetFrameRate() failed", err);
		return -1;
	}

	return 0;
}

int QT_FormatManager::getFramerate()
{
	OSErr err;
	Fixed fps;
	if((err = SGGetFrameRate(mQTDeviceDescriptor->getChannel(), &fps))) {
		logDebug("QT_FormatManager: SGGetFrameRate() failed", err);
		return -1;
	}

	return fps;
}

uint32_t QT_FormatManager::OSType2Fourcc(OSType pixel_format)
{
	// these are the supported fourcc's
	// other pixel-formats are decompressed by the VidCapManager to YUYV
	switch ( pixel_format )
	{
	case k32BGRAPixelFormat:
		return PIX_FMT_BGR32;
		break;
		
	case k32ARGBPixelFormat:
		return PIX_FMT_RGB32;
		break;

	case kYUVSPixelFormat:
		return PIX_FMT_YUYV;
		break;
		
	case k2vuyPixelFormat:
		return PIX_FMT_UYVY;
		break;
		
	case k16LE555PixelFormat:
		return PIX_FMT_RGB555;
		break;
		
	case k24RGBPixelFormat:
		return PIX_FMT_RGB24;
		break;
	}

	return 0;
}

OSType QT_FormatManager::Fourcc2OSType(uint32_t fourcc)
{
	switch (fourcc)
	{
	case PIX_FMT_BGR32:
		return k32BGRAPixelFormat;
		break;
		
	case PIX_FMT_RGB32:
		return k32ARGBPixelFormat;
		break;
		
	case PIX_FMT_YUYV:
		return kYUVSPixelFormat;
		break;
		
	case PIX_FMT_UYVY:
		return k2vuyPixelFormat;
		break;

	case PIX_FMT_RGB555:
		return k16LE555PixelFormat;
		break;
		
	case PIX_FMT_RGB24:
		return k24RGBPixelFormat;
		break;
	}
	
	return 0;
}


