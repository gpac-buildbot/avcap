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
#include <time.h>

#include "raw1394util.h"

#include "AVC_Reader.h"
#include "AVC_VidCapManager.h"
#include "AVC_DeviceDescriptor.h"
#include "AVC_FormatManager.h"
#include "IOBuffer.h"
#include "CaptureHandler.h"

using namespace avcap;

// Construction & Destruction

AVC_VidCapManager::AVC_VidCapManager(AVC_DeviceDescriptor* dd, AVC_FormatManager *fmt_mgr, int nbufs):
	mDeviceDescriptor(dd), 
	mFormatMgr(fmt_mgr), 
	mReader(0),
	mConnection(0),
	mSequence(0)
{
	mNumBufs = nbufs > 1 ? nbufs : 2;
	mNumBufs = mNumBufs <= MAX_BUFFERS ? mNumBufs : MAX_BUFFERS;
}

AVC_VidCapManager::~AVC_VidCapManager()
{
}

// initialize capture stuff

int AVC_VidCapManager::init()
{
	// find the device
	int port = -1;
	int node = discoverAVC(&port, &mDeviceDescriptor->getGUID());
	
	if(node == -1)
		return -1;

	// connect to the device
	if ( port != -1 ) {
		iec61883Connection::CheckConsistency( port, node );
		
		mConnection = new iec61883Connection(port, node);
			
		if(!mConnection)
			return -1;
		
		// and create the reader
		int channel = mConnection->GetChannel();
		mReader = new AVC_Reader( this, mFormatMgr, port, channel, mNumBufs);
	}
	
	return 0;
}

int AVC_VidCapManager::destroy()
{	
	// cleanup stuff
	delete mReader;
	mReader = 0;
	
	delete mConnection;
	mConnection = 0;
	
	return 0;
}


int AVC_VidCapManager::startCapture()
{
	// Guess what it does.
	mFormatMgr->flush();
	mReader->StartThread();
	
	return 0;
}

int AVC_VidCapManager::stopCapture()
{
	mReader->StopThread();		
	return 0;
}


int AVC_VidCapManager::enqueue(IOBuffer *io_buf)
{
	// enable the reader to reuse the buffer
	mReader->enqueue(io_buf);
	return 0;
}

IOBuffer* AVC_VidCapManager::dequeue()
{
	// not used by this capture-manager
	return 0;
}

void AVC_VidCapManager::registerCaptureHandler(CaptureHandler *handler) 
{
	// pass the registration to the reader
	mReader->registerCaptureHandler(handler);
	CaptureManager::registerCaptureHandler(handler);
}

void AVC_VidCapManager::removeCaptureHandler() 
{
	mReader->removeCaptureHandler(); 
	CaptureManager::removeCaptureHandler();
}
		
int AVC_VidCapManager::getNumIOBuffers()
{
	return mReader->getNumIOBuffers();
}

#endif // HAS_AVC_SUPPORT


