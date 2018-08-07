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
#include <errno.h>
#include <sys/ioctl.h>

#include "V4L2_FormatManager.h"
#include "DeviceDescriptor.h"
#include "uvc_compat.h"
#include "pwc-ioctl.h"
#include "log.h"

#ifdef AVCAP_HAVE_V4L2
#include <linux/videodev2.h>
#else
#include <linux/videodev.h>
#endif


using namespace avcap;

// Construction & Destruction

V4L2_FormatManager::V4L2_FormatManager(V4L2_DeviceDescriptor *dd):
	FormatManager((DeviceDescriptor*)dd)
{
}

V4L2_FormatManager::~V4L2_FormatManager()
{
}

void V4L2_FormatManager::query()
{
	// Find available formats and video standards.

	// query the video standards first
	queryVideoStandards();
	
	// the ivtv driver uses special io controls to set the pixel format or stream type
	// and doesn't support the VIDIOC_ENUM_FMT ioctl
	if(mDeviceDescriptor->getDriver() == DRIVER_IVTV) {
		Format *f = new Format("MPEG", V4L2_PIX_FMT_MPEG);
		
		logDebug("V4L2_FormatManager: found ivtv-Format " + f->getName());
		mFormats.push_back(f);
		queryResolutions(f);
	} else {
		// enumerate the formats
		for(int i = 0;;i++)
		{
			struct v4l2_fmtdesc dsc;
			memset(&dsc, 0, sizeof(v4l2_fmtdesc));
			dsc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			dsc.index = i;
			
			// finish, if there are no more formats
			int res = ioctl(mDeviceDescriptor->getHandle(), VIDIOC_ENUM_FMT, &dsc);
			if(res == -1)
				break;		
			
			logDebug(std::string("V4L2_FormatManager: found Format ") + (const char*) dsc.description);
			
			// create a new format object, if a new format was found
			struct Format *f = new Format((const char*)dsc.description,	dsc.pixelformat);
			queryResolutions(f);
			
			// and store it in the format list
			mFormats.push_back(f);
		}
		
		// no formats found, then probe some standard formats
		if(mFormats.size() == 0) {
			struct fmtdesc {
				char	description[32];
				__u32	pixelformat;
			};
			fmtdesc desc[] = {
				{"RGB", V4L2_PIX_FMT_RGB32},
				{"YUYV", V4L2_PIX_FMT_YUYV},
				{"UYVY", V4L2_PIX_FMT_UYVY},
				{"422P", V4L2_PIX_FMT_YUV422P},
				{"411P", V4L2_PIX_FMT_YUV411P},
				{"YVU9", V4L2_PIX_FMT_YVU410},
				{"YV12", V4L2_PIX_FMT_YVU420},
				{"MJPEG", V4L2_PIX_FMT_MJPEG},
				{"JPEG", V4L2_PIX_FMT_JPEG},
				{"MPEG", V4L2_PIX_FMT_MPEG}
			};
			
			for(int i = 0; i < 10; i++) {
				Format* f = new Format(desc[i].description, desc[i].pixelformat);
				queryResolutions(f);
				if(f->getResolutionList().size() == 0)
					delete f;
				else
					mFormats.push_back(f);
			}
		}
	}
	
	// synchronize the general format parameters with the driver settings
	int res = getParams();
	
	if(mFormats.size() == 0 && res != -1) {
		Format* f = new Format("UNKNOWN", mCurrentFormat);
		f->addResolution(mWidth, mHeight);
		mFormats.push_back(f);
	}
}

void V4L2_FormatManager::queryResolutions(Format* f)
{
	static unsigned int Resolutions[12][2] = 
	{
		{640, 480},
		{640, 360},
		{352, 288}, 
		{352, 240}, 
		{320, 240}, 
		{176, 144},
		{160, 120},
		{80, 60},
		{720, 480},
		{720, 576},
		{800, 600},
		{1280, 1024}
	};

	int ret = 0;
	struct v4l2_frmsizeenum fsize;

	memset(&fsize, 0, sizeof(fsize));
	
	fsize.index = 0;
	fsize.pixel_format = f->getFourcc();
	
	while ((ret = ioctl(mDeviceDescriptor->getHandle(), VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0) {
		if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
			switch(fsize.type) {
				case V4L2_FRMSIZE_TYPE_DISCRETE:
					f->addResolution(fsize.discrete.width, fsize.discrete.height);
				break;
				
				case V4L2_FRMSIZE_TYPE_CONTINUOUS:
					// just use some standard resolutions
					for(int i = 0; i < 11; i++)
						f->addResolution(Resolutions[i][0], Resolutions[i][1]);
				break;
				
				case V4L2_FRMSIZE_TYPE_STEPWISE:
					for(unsigned int y = fsize.stepwise.min_height; y <= fsize.stepwise.max_height; 
						y += fsize.stepwise.step_height) {
						for(unsigned int x = fsize.stepwise.min_width; x <= fsize.stepwise.max_width; 
							x += fsize.stepwise.step_width) {
							f->addResolution(x, y);
						}
					}
				break;
			}			
		}
		
		fsize.index++;
	}
	
	// the VIDIOC_ENUM_FRAMESIZES ioctl isn't supported, so probe some standard formats
	if(ret == -1 && fsize.index == 0) {
		// and return the success state
		for(int i = 0; i < 12; i++) {
			struct v4l2_format fmt;
			memset(&fmt, 0, sizeof(struct v4l2_format));

			fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			fmt.fmt.pix.width = Resolutions[i][0];
			fmt.fmt.pix.height = Resolutions[i][1];
			fmt.fmt.pix.pixelformat = f->getFourcc();
			
			if(ioctl(mDeviceDescriptor->getHandle(), VIDIOC_TRY_FMT, &fmt) == -1)
				continue;

			if(fmt.fmt.pix.width == Resolutions[i][0] && fmt.fmt.pix.height == Resolutions[i][1]
				&& fmt.fmt.pix.pixelformat == f->getFourcc())
				f->addResolution(Resolutions[i][0], Resolutions[i][1]);
		}
		
	}
}

int V4L2_FormatManager::setFormat(Format* f)
{
	// there is no choise of formats for the ivtv driver. only encoder params can 
	// be set
	if(mDeviceDescriptor->getDriver() == DRIVER_IVTV)
		return 0;
		
	if(f == 0)
		return -1;
	
	if(mCurrentFormat != f->getFourcc()) {
	      // formats are identified by their forcc
	      mCurrentFormat = f->getFourcc();
	      // something has changed
	      mModified = true;
	}
	
	return 0;
}

int V4L2_FormatManager::setFormat(unsigned int fourcc)
{
	for(ListType::const_iterator i = mFormats.begin(); i != mFormats.end(); i++) {
		Format	*f = *i;
		if(f->getFourcc() == fourcc)
			return setFormat(f);
	}
	
	return -1;
}

int V4L2_FormatManager::getParams()
{
	// Read out the current parameters from the driver

	struct v4l2_format	fmt;
	// and ask the driver which format is currently set
	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			
	int res = ioctl(mDeviceDescriptor->getHandle(), VIDIOC_G_FMT, &fmt);
			
	if(res == -1) 
		return -1;
		
	mWidth			= fmt.fmt.pix.width;
	mHeight			= fmt.fmt.pix.height;
	mBytesPerLine 	= fmt.fmt.pix.bytesperline;
	mCurrentFormat 	= fmt.fmt.pix.pixelformat;
	mImageSize 		= fmt.fmt.pix.sizeimage;
	
	return 0;
}


Format* V4L2_FormatManager::getFormat()
{
	// Return the format object that corresponds to the current format.

	Format *res = 0;

	// there is only one format for the ivtv driver
	if(mDeviceDescriptor->getDriver() == DRIVER_IVTV)
		return *(mFormats.begin());

	// synchronize params	
	if(flush() == -1)
		return 0;
	
	// and find the corresponding format object
	for(ListType::iterator it = mFormats.begin(); it != mFormats.end(); it++)		
	{
		if((*it)->getFourcc() == mCurrentFormat)
		{
			res = *it;
			break;
		}
	}

	return res;
}

int V4L2_FormatManager::setResolution(int w, int h)
{
	mWidth = w;
	mHeight = h;
	mModified = true;

	return 0;
}

int V4L2_FormatManager::getWidth()
{
	if(flush() == -1)
		return 0;

	return mWidth;
}

int V4L2_FormatManager::getHeight()
{
	if(flush() == -1)
		return 0;

	return mHeight;
}

int V4L2_FormatManager::setBytesPerLine(int bpl)
{
	mBytesPerLine = bpl;
	mModified = true;

	return 0;
}

int V4L2_FormatManager::getBytesPerLine()
{
	if(flush() == -1)
		return -1;

	return mBytesPerLine;
}

size_t V4L2_FormatManager::getImageSize()
{
	if(flush() == -1 || getParams() == -1)
		return 0;
	return mImageSize;	
}

int V4L2_FormatManager::flush()
{
	// Flush settings. Forward all modifications to the driver.

	// has something changed
	if(mModified) {
		// then propagate the changes to the driver
		struct v4l2_format fmt;
		memset(&fmt, 0, sizeof(struct v4l2_format));
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		int res = ioctl(mDeviceDescriptor->getHandle(), VIDIOC_G_FMT, &fmt);
		if (res == -1) {
		   return res;
		}
		
		fmt.fmt.pix.width = mWidth;
		fmt.fmt.pix.height = mHeight;
		// fmt.fmt.pix.bytesperline = mBytesPerLine;
		fmt.fmt.pix.pixelformat = mCurrentFormat;
		fmt.fmt.pix.field        = V4L2_FIELD_ANY;

		res = ioctl(mDeviceDescriptor->getHandle(), VIDIOC_S_FMT, &fmt);

		if (res != -1) {
			mModified = false;
		}  
		
		getParams();
	
		return res;
	}
	
	return 0;
}


int V4L2_FormatManager::tryFormat()
{
	// Test whether the driver accepts the format settings or not. Returns 0 if so and -1 if not.

	// something has changed
	if(mModified) {
		// then try to set the new parameters
		struct v4l2_format fmt;
		memset(&fmt, 0, sizeof(struct v4l2_format));
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		
		fmt.fmt.pix.width = mWidth;
		fmt.fmt.pix.height = mHeight;
		fmt.fmt.pix.bytesperline = mBytesPerLine;
		fmt.fmt.pix.pixelformat = mCurrentFormat;
		
		// and return the success state
		return ioctl(mDeviceDescriptor->getHandle(), VIDIOC_TRY_FMT, &fmt);
	}
	else return 0;
}

void V4L2_FormatManager::queryVideoStandards()
{
	// Query for available video standards.

	// delete the old standards-entries
	for(VideoStandardList::iterator it = mStandards.begin(); it != mStandards.end(); it++)
		delete *it;
	
	// clear the list
	mStandards.clear();
	
	// and query the standards for the current input
	for(int index = 0;; index++) {
		// enumerate standards
		struct v4l2_standard	std;
		memset(&std, 0, sizeof (v4l2_standard));
		std.index = index;

		if (ioctl(mDeviceDescriptor->getHandle(), VIDIOC_ENUMSTD, &std) < 0)
			break;
			
		// there is a new standard so create a VideoStandard object
		struct VideoStandard *vs = new VideoStandard((const char*) std.name, std.id);
		
		// and store it in the standard list
		mStandards.push_back(vs);
		
		logDebug("\tConnectorManager::queryVideoStandards(): Standard: " + vs->name);
	}
}

const VideoStandard* V4L2_FormatManager::getVideoStandard()
{
	// Return the VideoStandard object that corresponds to the current standard.

	v4l2_std_id id;
	memset(&id, 0, sizeof(v4l2_std_id));

	// get the current standard	
	if (ioctl(mDeviceDescriptor->getHandle(), VIDIOC_G_STD, &id) < 0)
		return 0;
	
	// and find the corresponding object
	for(VideoStandardList::iterator it = mStandards.begin(); it != mStandards.end(); it++)	
		if((*it)->id == id)
			return *it;
			
	return 0;
}

int V4L2_FormatManager::setVideoStandard(const VideoStandard* std)
{
	// Set the video standard

	v4l2_std_id id;
	id = std->id;
	
	return ioctl(mDeviceDescriptor->getHandle(), VIDIOC_S_STD, &id);
}

int V4L2_FormatManager::setFramerate(int fps)
{
#ifndef AVCAP_HAVE_V4L2
	// there is a special way to set the framerate for the pwc driver
	if(mDeviceDescriptor->getDriver() == "pwc") {
		struct video_window vwin;
		memset(&vwin, 0, sizeof(vwin));
 		ioctl(mDeviceDescriptor->getHandle(), VIDIOCGWIN, &vwin);
		
		// set new framerate
		vwin.flags &= ~PWC_FPS_FRMASK;
		vwin.flags |= (fps << PWC_FPS_SHIFT);
		 
		return ioctl(mDeviceDescriptor->getHandle(), VIDIOCSWIN, &vwin);
	} 
#endif
	
	struct v4l2_streamparm setfps;  
	memset(&setfps, 0, sizeof(struct v4l2_streamparm));
	
	setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	setfps.parm.capture.timeperframe.numerator = 1;
	setfps.parm.capture.timeperframe.denominator = fps;
	
	return ioctl(mDeviceDescriptor->getHandle(), VIDIOC_S_PARM, &setfps); 
}

int V4L2_FormatManager::getFramerate()
{
#ifndef AVCAP_HAVE_V4L2
	// there is a special way to get the framerate for the pwc driver
	if(mDeviceDescriptor->getDriver() == "pwc") {
		struct video_window vwin;

		ioctl(mDeviceDescriptor->getHandle(), VIDIOCGWIN, &vwin);
		if (vwin.flags & PWC_FPS_FRMASK)
    		return ((vwin.flags & PWC_FPS_FRMASK) >> PWC_FPS_SHIFT);
		else
    		return 0;
	}
#endif
	
	struct v4l2_streamparm setfps;  
	memset(&setfps, 0, sizeof(struct v4l2_streamparm));
	
	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOC_G_PARM, &setfps) == -1)
		return -1;
		
	return (int) (setfps.parm.capture.timeperframe.denominator/setfps.parm.capture.timeperframe.numerator);
}

