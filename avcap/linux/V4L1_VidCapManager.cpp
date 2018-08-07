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
#include <time.h>
#include <math.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <linux/types.h>

#include "avcap-config.h"
#ifndef AVCAP_HAVE_V4L2
#include <linux/videodev.h>

#include "V4L1_VidCapManager.h"
#include "V4L1_DeviceDescriptor.h"
#include "V4L1_FormatManager.h"
#include "IOBuffer.h"
#include "CaptureHandler.h"


using namespace avcap;

// Construction & Destruction

V4L1_VidCapManager::V4L1_VidCapManager(V4L1_DeviceDescriptor* dd, V4L1_FormatManager *fmt_mgr, int nbufs):
	mDeviceDescriptor(dd), 
	mFormatMgr(fmt_mgr), 
	mMethod(IO_METHOD_NOCAP), 
	mThread(0), 
	mSequence(0),
	mAvailableBuffers(0)
{
	mNumBufs = nbufs > 1 ? nbufs : 2;
	mNumBufs = mNumBufs <= MAX_BUFFERS ? mNumBufs : MAX_BUFFERS;
	mFinish = 0;
}

V4L1_VidCapManager::~V4L1_VidCapManager()
{
	clearBuffers();
}

int V4L1_VidCapManager::init()
{
int res = -1;

	// init the lock
	pthread_mutex_init(&mLock, 0);
	// default method
	mMethod = IO_METHOD_NOCAP;
	
	if(mDeviceDescriptor->isVideoCaptureDev()) 	{
		// device supports streaming IO with mmaped or user buffers
		if(mDeviceDescriptor->isStreamingDev())
			mMethod = IO_METHOD_MMAP;
		else
			mMethod = IO_METHOD_READ;
	}

	// method dependend initialization	
	switch(mMethod)
	{
		case IO_METHOD_READ:
		break;
		
		case IO_METHOD_MMAP:
		break;
	}
	
	return res;
}


int V4L1_VidCapManager::start_read()
{
	// start capture if read() IO method is used.
	int size = mFormatMgr->getImageSize();
	
	// allocate IOBuffers for the images
	for(int i = 0; i < mNumBufs; i++)
	{
		void *ptr = new char[size];
		
		IOBuffer *buf = new IOBuffer(this, ptr, size);
		mBuffers.push_back(buf);
	}

	// store the current time to create a propper time stamp
	gettimeofday(&mStartTime, 0);

	return 0;
}

// stop capturing for the read() IO-method

int V4L1_VidCapManager::stop_read()
{
	return 0;
}


int V4L1_VidCapManager::start_mmap()
{
	// start cpturing using the mmap IO-method.

	struct 	video_mbuf vb;
	memset(&vb, 0, sizeof(vb));
	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOCGMBUF, &vb) == -1)
		return -1;
	
	mVideobuf = (unsigned char*) mmap(0, vb.size, PROT_READ|PROT_WRITE, MAP_SHARED, mDeviceDescriptor->getHandle(), 0);
	mVideobufSize = vb.size;
	
	if((unsigned char*) -1 == mVideobuf)
		return -1;
		
  	mHeight = mFormatMgr->getHeight();
  	mWidth = mFormatMgr->getWidth();
  	mPalette = mFormatMgr->getPalette();
	mNumBufs = vb.frames;

	// create and enqueue the buffers
	for(int i = 0; i < mNumBufs; i++) {
		IOBuffer* buf = new IOBuffer(this, mVideobuf + vb.offsets[i], mFormatMgr->getImageSize(), i);
		mBuffers.push_back(buf);
		buf->setState(IOBuffer::STATE_USED);
		enqueue(buf);
	}

	return 0;
}

int V4L1_VidCapManager::stop_mmap()
{
	// mmap-specific stop capture method.
	clearBuffers();	
	munmap(mVideobuf, mVideobufSize); 
	return 0;
}

int V4L1_VidCapManager::destroy()
{
int res = 0;

	// cleanup stuff
	switch(mMethod)
	{
		case IO_METHOD_READ:
		break;
		
		case IO_METHOD_MMAP:
		break;
	}

	// destroy mutex
	pthread_mutex_destroy(&mLock);
		
	return res;
}

void V4L1_VidCapManager::run(void* mgr)
{
V4L1_VidCapManager* io_mgr = (V4L1_VidCapManager*) mgr;

	// capture main loop

	// run the loop until finish flag is set
	while(!io_mgr->mFinish)
	{
		fd_set fds;
		struct timeval tv;

		FD_ZERO(&fds);
		FD_SET (io_mgr->mDeviceDescriptor->getHandle(), &fds);

		// Timeout 1 sec.
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		// wait until data is available or time is running out		
		int res = select (io_mgr->mDeviceDescriptor->getHandle() + 1, &fds, 0, 0, &tv);

		// an error occured
		if (-1 == res)
		{
			if (EINTR == errno)
				continue;
		}

		// get the current data buffer
		/*
		timeval start, end;
		gettimeofday(&start, 0);
		*/
		IOBuffer *io_buf = io_mgr->dequeue();
		/*
		gettimeofday(&end, 0);		
		std::cerr<<"capturing took: "<<(end.tv_usec- start.tv_usec)/1000.0<<"   ms\n";
		*/
		
		if(io_buf)
		{
			// test whether finish flag has been set in between
			if (!io_mgr->mFinish)
			{
				// and call the capture handler if one is registered
				if(io_mgr->getCaptureHandler()) {
					// std::cout<<"Calling capture-handler for #"<<io_buf->getSequence()<<"\n";
			 		io_mgr->getCaptureHandler()->handleCaptureEvent(io_buf);
				}
			 	else
			 		io_mgr->enqueue(io_buf); // otherwise enqueue buffer directly
			}
			else
				io_mgr->enqueue(io_buf);
		}
		
		// wait 20 ms to give clients the chance to 
		// free a buffer without polling on select.
		usleep((int)(1000.0*20));
	}	
}

int V4L1_VidCapManager::startCapture()
{
int res = 0;
	
	// is the capture thread already running?
	if(mThread != 0)
		return -1;
	
	// reset values
	mFinish = 0;	
	mBuffers.clear();
	mSequence = 0;
		
	// call the IO-method-specific start-method
	switch(mMethod)
	{
		case IO_METHOD_READ:
			res = start_read();
		break;
		
		case IO_METHOD_MMAP:
			res = start_mmap();
		break;
	}

	// create and start a new pthread
	mThread = new pthread_t; 
	pthread_create( mThread, 0,  (void* (*)(void*)) &V4L1_VidCapManager::run, (void*) this );
	
	int start = 1;
	res = ioctl(mDeviceDescriptor->getHandle(), VIDIOCCAPTURE, &start);
	return res;
}

int V4L1_VidCapManager::stopCapture()
{
int res = 0;
int stop = 0;

	res = ioctl(mDeviceDescriptor->getHandle(), VIDIOCCAPTURE, &stop);

	// not capturing 
    if(!mThread)
    	return -1;
    
	pthread_mutex_lock(&mLock);
	mFinish = 1;
	pthread_mutex_unlock(&mLock);

	// and wait till thread has finished
	pthread_join ( *mThread, 0);
	// delete the thread
	delete mThread;
	mThread = 0;

	// call the IO-method specific stop mehtod
	switch(mMethod)
	{
		case IO_METHOD_READ:
			res = stop_read();
		break;
		
		case IO_METHOD_MMAP:
			res = stop_mmap();
		break;
	}

	// iterate the buffer list and mark all buffers UNUSED
	for(IOBufList_t::iterator it = mBuffers.begin(); it != mBuffers.end(); it++)
	{
		IOBuffer *buf = *it;
		if(buf)
			(buf->setState(IOBuffer::STATE_UNUSED));
	}

	// and kick the buffers
	clearBuffers();
	
	return res;
}

int V4L1_VidCapManager::enqueue(IOBuffer *io_buf)
{
	int ret = 0;

	// enqueues an IOBuffer into the queue of incomming buffers, 
	// e.g. enables the driver to reuse the buffer

	if(mFinish)
		return 0;
	
	if(io_buf == 0 || io_buf->getState() == IOBuffer::STATE_UNUSED) 
		return -1;
	
	switch(mMethod)
	{        
        case IO_METHOD_READ:
        	// set the buffer state to unused 
			pthread_mutex_lock(&mLock);
			if(mFinish) {
				pthread_mutex_unlock(&mLock);
				// if the thread has already finished in between clear the buffer
				clearBuffers();	
				return -1;
			}

			io_buf->setState(IOBuffer::STATE_UNUSED);
			mAvailableBuffers++;
			pthread_mutex_unlock(&mLock);
		break;
		
		case IO_METHOD_MMAP:
        	// set the buffer state to unused 	
			pthread_mutex_lock(&mLock);
			if(mFinish) {
				pthread_mutex_unlock(&mLock);
				// if the thread has already finished in between clear the buffer
				clearBuffers();	
				return -1;
			}

			io_buf->setState(IOBuffer::STATE_UNUSED);
			mAvailableBuffers++;
				
		  	struct video_mmap vmm;
		  	memset(&vmm, 0, sizeof(vmm));
			  	
		    vmm.frame  = io_buf->getIndex();
		    vmm.height = mHeight;
		    vmm.width  = mWidth;
		    vmm.format = mPalette;
			    
		    ret = ioctl(mDeviceDescriptor->getHandle(), VIDIOCMCAPTURE, &vmm);
		    if(ret != -1) {
			    mCaptureIndices.push_back(io_buf->getIndex());
		    }
			 
			pthread_mutex_unlock(&mLock);
		break;
	}
	
	return ret;
}

IOBuffer* V4L1_VidCapManager::dequeue()
{
IOBuffer *res = 0;

	// populate an unused IOBuffer with the currently captured data
	switch(mMethod)
	{        
        case IO_METHOD_READ:
        	pthread_mutex_lock(&mLock);
        	
			if(mFinish) {
				pthread_mutex_unlock(&mLock);
				// if the thread has already finished in between clear the buffer
				clearBuffers();	
				return 0;
			}

        	// find an unused buffer
        	for(IOBufList_t::iterator it = mBuffers.begin(); it != mBuffers.end(); it++)
        	{
        		res = *it;
        		// first fit
        		if(res->getState() == IOBuffer::STATE_UNUSED)
        			break;
        	}
        	
        	// did we find a buffer
        	if(res->getState() == IOBuffer::STATE_UNUSED)
        	{
        		// then compute the timestamp from the start-time, the framerate and the sequence number
        		struct timeval	tv;
				gettimeofday(&tv, 0);
	        		
	        	// read the captured data
				int n = read(mDeviceDescriptor->getHandle(), res->getPtr(), res->getSize() );
	
				// and update the buffer parameter
				if(n > 0) {
					// std::cout<<"found empty buffer "<<res<<" State before: "<< res->getState();
					res->setParams(n, IOBuffer::STATE_USED, tv, mSequence++ + 1);
					mAvailableBuffers--;
					// std::cout<<" and after: "<<res->getState()<<"\n";
				}
				else
					res = 0;
        	}
        	else
				res = 0;
			pthread_mutex_unlock(&mLock);
		break;
		
		case IO_METHOD_MMAP:
			pthread_mutex_lock(&mLock);
			if(mFinish) {
				pthread_mutex_unlock(&mLock);
				// if the thread has already finished in between clear the buffer
				clearBuffers();	
				return 0;
			}

			int index = -1;
			if(mCaptureIndices.size() > 0) {
				index = mCaptureIndices.front();
						
				// and find the next buffer object
				res = findBuffer(index);
				int ret;
				while((ret = ioctl(mDeviceDescriptor->getHandle(), VIDIOCSYNC, &index)) < 0 && 
					(errno == EAGAIN || errno == EINTR));
				
				if(ret != -1) {
					mSequence++;
					timeval tv;
					gettimeofday(&tv, 0);
		
					// set the buffer parameters
					res->setParams(res->getSize(), IOBuffer::STATE_USED, tv, mSequence + 1);
					mCaptureIndices.pop_front();
					mAvailableBuffers--;
					
				} else {
					res = 0;
				}
			}
			
			pthread_mutex_unlock(&mLock);
		break;
	}
	
	return res;
}

IOBuffer* V4L1_VidCapManager::findBuffer(int index)
{
IOBuffer	*io_buf = 0;
	
	// Find a buffer by its unique index.
	
	// iterate the list and return the correct buffer object
	for(IOBufList_t::iterator it = mBuffers.begin(); it != mBuffers.end(); it++)
	{
		io_buf = *it;
		if(io_buf->getIndex() == index)
			break;
	}

	return io_buf;
}

int V4L1_VidCapManager::getUsedBufferCount()
{
int res = 0;

	// Return the number of currently used buffers.

	// iterate the list and count the used buffers
	for(IOBufList_t::iterator it = mBuffers.begin(); it != mBuffers.end(); it++)
		if (*it)
			if((*it)->getState() == IOBuffer::STATE_USED)
				res++;
	
	return res;
}

void V4L1_VidCapManager::clearBuffers()
{
	// Unmmaps and deletes all unused buffers.

	// synchronize access to the buffer list cause multiple threads can access them
	// when they release a buffer.
	pthread_mutex_lock(&mLock);

	// iterate the buffer list
	for(IOBufList_t::iterator it = mBuffers.begin(); it != mBuffers.end(); it++)
	{
		IOBuffer *buf = *it;
		if(buf)
		{
			if(buf->getState() == IOBuffer::STATE_UNUSED)
			{
				// the buffer isn't needed anymore
				switch(mMethod)
				{
					case IO_METHOD_READ:
						// so delete
						// TODO: data-type should be uint8_t
						delete[] ((char*) buf->getPtr());
					break;
					
					case IO_METHOD_MMAP:
						// or munmap it
					break;
				}
				
				// and delete the buffer
				delete buf;
				*it = 0;
			}
		}
	}

	// if no buffer is there anymore clear the list
	if(getUsedBufferCount() == 0)
		mBuffers.clear();	
	
	// and unlock
	pthread_mutex_unlock(&mLock);
}

int V4L1_VidCapManager::getNumIOBuffers()
{
	return mAvailableBuffers;
}

#endif


