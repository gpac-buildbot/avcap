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


#ifdef HAS_AVC_SUPPORT

#include <iostream>
#include <sys/ioctl.h>

#include "raw1394util.h"
#include "ieee1394io.h"
#include "frame.h"

#include "AVC_FormatManager.h"
#include "AVC_DeviceDescriptor.h"

using namespace avcap;

// Construction & Destruction

AVC_FormatManager::AVC_FormatManager(AVC_DeviceDescriptor *dd):
	FormatManager((DeviceDescriptor *)dd),
	mIsPal(true)
{
}

AVC_FormatManager::~AVC_FormatManager()
{
}

void AVC_FormatManager::query()
{
	// Find available formats and video standards.

	int port = -1;
	AVC_DeviceDescriptor* dd = dynamic_cast<AVC_DeviceDescriptor*>(mDeviceDescriptor);
	std::auto_ptr<iec61883Reader> reader;
	std::auto_ptr<iec61883Connection> connection;
	int node = discoverAVC(&port, &dd->getGUID());
	
	if(node == -1)
		return;

	if ( port != -1 ) {
		// capture a single frame and retrieve it's properties
		iec61883Connection::CheckConsistency( port, node );
		connection = std::auto_ptr<iec61883Connection>(new iec61883Connection(port, node));
			
		int channel = connection->GetChannel();
		reader = std::auto_ptr<iec61883Reader>(new iec61883Reader( port, channel));

		reader->StartThread();
		
		// Wait for the reader to indicate that something has happened
			
		if(!reader->WaitForAction(5)) {
			std::cerr<<"AVC_FormatManager: Timed out retrieving format.\n";
		} else {
			// Get the next frame
			Frame* frame = reader->GetFrame();
			if(!frame) {
					std::cout<<"AVC_FormatManager: Error retrieving format parameters.\n";
			}

			// get format-properties
			mWidth = frame->GetWidth();
			mHeight = frame->GetHeight();
			mBytesPerLine = mWidth;
			mImageSize = mWidth*mHeight*3;
			
			// create the formats
			Format* yuv = new Format("YUYV", v4l2_fourcc('Y','U','Y','V'));
			mFormats.push_back(yuv);

			Format* rgb = new Format("RGB", v4l2_fourcc('R', 'G', 'B', 0));
			mFormats.push_back(rgb);
			
			setFormat(yuv);
			
			// and the proper video-standards
			mIsPal = frame->IsPAL();
			if(mIsPal) {
				VideoStandard* pal = new VideoStandard("PAL", VideoStandard::PAL);
				mStandards.push_back(pal);
				yuv->addResolution(720, 576);			
				rgb->addResolution(720, 576);			
			} else {
				VideoStandard* ntsc = new VideoStandard("NTSC", VideoStandard::NTSC);
				mStandards.push_back(ntsc);						
				yuv->addResolution(720, 480);			
				rgb->addResolution(720, 480);			
			}
			
			// release the frame 
			reader->DoneWithFrame( frame );
		}
		
		// and stop capturing
		reader->StopThread();
	}
}

int AVC_FormatManager::setResolution(int w, int h) 
{
	// resolution is not realy setable, so accept only native resolution
	if(mIsPal && w == 720 && h == 576)
		return 0;

	if(!mIsPal && w == 720 && h == 480)
		return 0;
	
	return -1;
}

#endif

