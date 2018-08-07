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
#include <sys/ioctl.h>

#include "avcap-config.h"
#ifndef AVCAP_HAVE_V4L2
#include <linux/videodev.h>

#include "V4L1_FormatManager.h"
#include "V4L1_DeviceDescriptor.h"
#include "uvc_compat.h"
#include "pwc-ioctl.h"
#include "ProbeValues.h"

using namespace avcap;

const struct V4L1_FormatManager::fmtdesc V4L1_FormatManager::mDescriptors[V4L1_FormatManager::mNumDescriptors] = {
			// TODO: use FOURCC-macro defined in FormatManager.h to create fourcc
			{"GREY", VIDEO_PALETTE_GREY, V4L2_PIX_FMT_GREY, 1.0f},
			{"HI240", VIDEO_PALETTE_HI240, V4L2_PIX_FMT_HI240, 4.0f},
			{"RGB565", VIDEO_PALETTE_RGB565, V4L2_PIX_FMT_RGB565, 2.0f},
			{"RGB", VIDEO_PALETTE_RGB24, V4L2_PIX_FMT_RGB24, 3.0f},
			{"RGBA", VIDEO_PALETTE_RGB32, V4L2_PIX_FMT_RGB32, 4.0f},
			{"RGB555", VIDEO_PALETTE_RGB555, V4L2_PIX_FMT_RGB555, 2.0f},
			{"YUV422", VIDEO_PALETTE_YUV422, v4l2_fourcc('Y','4','2','2'), 2.0f},
			{"YUV420", VIDEO_PALETTE_YUV420, v4l2_fourcc('Y','4','2','0'), 1.5f},
			{"Y41P", VIDEO_PALETTE_YUV411, V4L2_PIX_FMT_Y41P, 1.5f},
			{"RAW", VIDEO_PALETTE_RAW, v4l2_fourcc('R','A','W','0'), 4.0f},
			{"YV16", VIDEO_PALETTE_YUV422P, V4L2_PIX_FMT_YUV422P, 2.0f},
			{"YUV411P", VIDEO_PALETTE_YUV411P, V4L2_PIX_FMT_YUV411P, 1.5f},
			{"YU12", VIDEO_PALETTE_YUV420P, V4L2_PIX_FMT_YUV420, 1.5f},
			{"YUV410P", VIDEO_PALETTE_YUV410P, V4L2_PIX_FMT_YUV410, 1.5f},
			{"PLANAR", VIDEO_PALETTE_PLANAR, v4l2_fourcc('P','L','N','R'), 4.0f},
			{"COMPONENT", VIDEO_PALETTE_COMPONENT, v4l2_fourcc('C','O','M','P'), 4.0f},
			{"YUYV", VIDEO_PALETTE_YUYV, V4L2_PIX_FMT_YUYV, 2.0f},
			{"UYVY", VIDEO_PALETTE_UYVY, V4L2_PIX_FMT_UYVY, 2.0f},
		};

// Construction & Destruction

V4L1_FormatManager::V4L1_FormatManager(V4L1_DeviceDescriptor *dd):
	FormatManager((DeviceDescriptor*)dd),
	mIsOVFX2(false)
{
}

V4L1_FormatManager::~V4L1_FormatManager()
{
}

void V4L1_FormatManager::query()
{
	// Find available formats and video standards.
	struct video_picture vp;	
	memset(&vp, 0, sizeof(vp));
	if(ioctl(mDeviceDescriptor->getHandle(),VIDIOCGPICT, &vp) == -1)
		return;
	
	// make a copy to restore values after the query
	struct video_picture old_vp;
	memcpy(&old_vp, &vp, sizeof(vp));

	struct video_window old_vw;
	memset(&old_vw, 0, sizeof(old_vw));
	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCGWIN, &old_vw) == -1)
		return;
	
	if(mDeviceDescriptor->getDriver() == "OVFX2 USB Camera") {
		// iBot2 needs extra threatment, because BGR3 isn't provided in V4L1
		// so map the RGB24-palette to the fourcc 'BGR3'
		Format* f = new Format("BGR3", V4L2_PIX_FMT_BGR24);
		mFormats.push_back(f);
		mIsOVFX2 = true;
	} else {
		// probe some standard formats
		for(int i = 0; i < 18; i++) {
			vp.palette = mDescriptors[i].palette;
			
			if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCSPICT, &vp) == -1)
				continue;
				
			memset(&vp, 0, sizeof(vp));
			if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCGPICT, &vp) == -1)
				continue;
			
			if(vp.palette != mDescriptors[i].palette)
				continue;
			
			mFormats.push_back(new Format(mDescriptors[i].description, mDescriptors[i].fourcc));
		}
	}
	
	queryResolutions();
	
	// restore the old values
	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCSWIN, &old_vw) == -1)
			return;
	
	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCSPICT, &old_vp) == -1)
			return;
	
	// synchronize the general format parameters with the driver settings
	getParams();
	
	if(mFormats.size())
		setFormat(mFormats.front());
}

void V4L1_FormatManager::queryResolutions()
{
	V4L1_DeviceDescriptor* v4l1_dd = dynamic_cast<V4L1_DeviceDescriptor*>(mDeviceDescriptor);
	if(!v4l1_dd)
		return;
		
	V4L1_DeviceDescriptor::bounds& bounds = v4l1_dd->getBounds();

	struct video_window vw;
	memset(&vw, 0, sizeof(vw));
	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCGWIN, &vw) == -1)
		return;
	
	// probe some standard resolutions
	for(int i = 0; i < NumResolutions; i++) {
		// check bounds
		if(!(Resolutions[i][0] >= bounds.minwidth && Resolutions[i][0] <= bounds.maxwidth && 
		Resolutions[i][1] >= bounds.minheight && Resolutions[i][1] <= bounds.maxheight))
			continue;
			
		vw.width = Resolutions[i][0];
		vw.height = Resolutions[i][1];
		
		if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCSWIN, &vw) == -1)
			continue;

		memset(&vw, 0, sizeof(vw));		
		if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCGWIN, &vw) == -1)
			continue;
			
		if(vw.width == Resolutions[i][0] && vw.height == Resolutions[i][1]) {
			for(ListType::const_iterator fmt_it = mFormats.begin(); fmt_it != mFormats.end(); fmt_it++)
				(*fmt_it)->addResolution(Resolutions[i][0], Resolutions[i][1]);
		}
	}
}

__u16 V4L1_FormatManager::getPalette(__u32 fourcc)
{
	if(mIsOVFX2)
		return VIDEO_PALETTE_RGB24;
		
	for(int i = 0; i < mNumDescriptors; i++) {
		if(mDescriptors[i].fourcc == fourcc)
			return mDescriptors[i].palette;
	}
	
	return 0;
}

__u16 V4L1_FormatManager::getPalette()
{
	return getPalette(mCurrentFormat);
}

int V4L1_FormatManager::setFormat(Format* f)
{
	struct video_picture vp;	
	memset(&vp, 0, sizeof(vp));
	if(ioctl(mDeviceDescriptor->getHandle(),VIDIOCGPICT, &vp) == -1)
		return -1;
	
	vp.palette = getPalette(f->getFourcc());
	if(ioctl(mDeviceDescriptor->getHandle(),VIDIOCSPICT, &vp) == -1)
		return -1;
		
	return getParams();
}

int V4L1_FormatManager::setFormat(__u32 fourcc)
{
	for(ListType::const_iterator i = mFormats.begin(); i != mFormats.end(); i++) {
		Format	*f = *i;
		if(f->getFourcc() == fourcc)
			return setFormat(f);
	}
	
	return -1;
}


int V4L1_FormatManager::getParams()
{
	// Read out the current parameters from the driver
	// ask the driver which format and capture window are currently set
	struct video_window	vw;
	memset(&vw, 0, sizeof(vw));			
	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCGWIN, &vw) == -1)
		return -1;
		
	mWidth			= vw.width;
	mHeight			= vw.height;
	mBytesPerLine 	= mWidth;

	struct video_picture vp;	
	memset(&vp, 0, sizeof(vp));
	if(ioctl(mDeviceDescriptor->getHandle(),VIDIOCGPICT, &vp) == -1)
		return -1;

	int dsc_index = 0;	
	for(int i = 0; i < mNumDescriptors; i++)
		if(mDescriptors[i].palette == vp.palette) {
			dsc_index = i;
			break;
		}

	mImageSize 		= (int) (mDescriptors[dsc_index].sizefactor*mWidth*mHeight);
	mCurrentFormat 	= mDescriptors[dsc_index].fourcc;
	
	return 0;
}

Format* V4L1_FormatManager::getFormat()
{
Format *res = 0;

	// Return the format object that corresponds to the current format.
	// find the current format object
	for(ListType::iterator it = mFormats.begin(); it != mFormats.end(); it++) {
		if((*it)->getFourcc() == mCurrentFormat) {
			res = *it;
			break;
		}
	}

	return res;
}

int V4L1_FormatManager::setResolution(int w, int h)
{
	mWidth = w;
	mHeight = h;

	struct video_window	vw;
	memset(&vw, 0, sizeof(vw));			

	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCGWIN, &vw) == -1)
		return -1;

	vw.width = mWidth;
	vw.height = mHeight;

	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCSWIN, &vw) == -1)
		return -1;
		
	return getParams();	
}

int V4L1_FormatManager::getWidth()
{
	return mWidth;
}

int V4L1_FormatManager::getHeight()
{
	return mHeight;
}

int V4L1_FormatManager::setBytesPerLine(int bpl)
{
	return 0;
}

int V4L1_FormatManager::getBytesPerLine()
{
	return mBytesPerLine;
}

// Get total bytes needed to store one image with the current settings. 
// This is an upper bound not the correct value.
size_t V4L1_FormatManager::getImageSize()
{
	return mImageSize;	
}

int V4L1_FormatManager::flush()
{
	return 0;
}


const VideoStandard* V4L1_FormatManager::getVideoStandard()
{
	return 0;
}

int V4L1_FormatManager::setVideoStandard(const VideoStandard* std)
{
	return 0;
}

int V4L1_FormatManager::setFramerate(int fps)
{
	int res = 0;
	
	if(mDeviceDescriptor->getDriver().find("Philips") != std::string::npos) {
		struct video_window vwin;
		memset(&vwin, 0, sizeof(vwin));
		ioctl(mDeviceDescriptor->getHandle(), VIDIOCGWIN, &vwin);
		
		/* Set new framerate */
		vwin.flags &= ~PWC_FPS_FRMASK;
		vwin.flags |= (fps << PWC_FPS_SHIFT);
		 
		res = ioctl(mDeviceDescriptor->getHandle(), VIDIOCSWIN, &vwin);
	} 

	return res; 
}

int V4L1_FormatManager::getFramerate()
{
	int res = 0;
	
	if(mDeviceDescriptor->getDriver().find("Philips") != std::string::npos) {
		struct video_window vwin;

		ioctl(mDeviceDescriptor->getHandle(), VIDIOCGWIN, &vwin);
		if (vwin.flags & PWC_FPS_FRMASK)
    		res = ((vwin.flags & PWC_FPS_FRMASK) >> PWC_FPS_SHIFT);
	}

	return res;
}

#endif


