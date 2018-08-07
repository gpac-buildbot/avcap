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

#include <sys/time.h>
#include <time.h>

#include "AVC_Reader.h"
#include "AVC_VidCapManager.h"
#include "AVC_FormatManager.h"
#include "IOBuffer.h"
#include "frame.h"
#include "CaptureHandler.h"

using namespace avcap;

AVC_Reader::AVC_Reader(AVC_VidCapManager* cap_mgr, AVC_FormatManager* fmt_mgr, int port, int channel, int num_bufs):
	iec61883Reader(port, channel),
	mVidCapMgr(cap_mgr),
	mFormatMgr(fmt_mgr),
	mCaptureHandler(0),
	mSequence(1),
	mAvailableBuffers(num_bufs)
{
	// create the buffers
	size_t size = mFormatMgr->getImageSize();
	for(int i = 0; i < num_bufs; i++) {
		char* ptr = new char[size];
		IOBuffer* io_buf = new IOBuffer(mVidCapMgr, ptr, size, i);
		mBuffers.push_back(io_buf);
	}
}

AVC_Reader::~AVC_Reader()
{
	// delete the buffers
	for(BufferList_t::iterator i = mBuffers.begin(); i != mBuffers.end(); i++) {
		IOBuffer* io_buf = *i;
		delete[] (char*) io_buf->getPtr();
		delete io_buf;
	}
}

void AVC_Reader::registerCaptureHandler(CaptureHandler *handler)
{
	mCaptureHandler = handler;
}

void AVC_Reader::removeCaptureHandler()
{
	mCaptureHandler = 0;
}

void AVC_Reader::TriggerAction()
{
	// get the captured frame
	Frame* frame = GetFrame();
	
	// sanity-checks
	if(!frame)
		return;
		
	if(!frame->IsComplete()) {
		DoneWithFrame(frame);
		return;
	}
	
	// find a free IOBuffer
	IOBuffer* io_buf = 0;
	for(BufferList_t::iterator i = mBuffers.begin(); i != mBuffers.end(); i++) {
		IOBuffer* tmp_buf = *i;
		
		if(tmp_buf->getState() == IOBuffer::STATE_UNUSED)  {
			io_buf = tmp_buf;
			break;
		}
	}
	
	// decode the frame according to the desired format
	if(io_buf) {
		if(mFormatMgr->getFormat()->getName() == "YUYV" )
			frame->ExtractYUV(io_buf->getPtr());

		if(mFormatMgr->getFormat()->getName() == "RGB" ) {
			frame->ExtractRGB(io_buf->getPtr());
		}

		// add time-stamp and sequence
		timeval tv;
		gettimeofday(&tv, 0);
		io_buf->setParams(mFormatMgr->getImageSize(), IOBuffer::STATE_USED, tv, mSequence++);
		mAvailableBuffers--;
		
		// call the capture-handler
		if(mCaptureHandler)
			mCaptureHandler->handleCaptureEvent(io_buf);
	}
	
	// and release the frame
	DoneWithFrame(frame);
}

void AVC_Reader::enqueue(IOBuffer* io_buf)
{
	// we got back a buffer, so mark it unused
	io_buf->setState(IOBuffer::STATE_UNUSED);
	mAvailableBuffers++;
}

#endif // HAS_AVC_SUPPORT
