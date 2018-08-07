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
#include <unistd.h>

#include "V4L2_VidCapManager.h"
#include "V4L2_DeviceDescriptor.h"
#include "FormatManager.h"
#include "IOBuffer.h"
#include "CaptureHandler.h"
#include "log.h"

#ifdef AVCAP_HAVE_V4L2
#include <linux/videodev2.h>
#else
#include <linux/videodev.h>
#endif

using namespace avcap;

// Construction & Destruction

V4L2_VidCapManager::V4L2_VidCapManager(V4L2_DeviceDescriptor* dd, FormatManager *fmt_mgr, int nbufs):
	mDeviceDescriptor(dd), 
	mFormatMgr(fmt_mgr), 
	mMethod(IO_METHOD_NOCAP), 
	mThread(0), 
	mSequence(0),
	mDTNumerator(0),
	mDTDenominator(0),
	mAvailableBuffers(0)
{
	mNumBufs = nbufs > 1 ? nbufs : 2;
	mNumBufs = mNumBufs <= MAX_BUFFERS ? mNumBufs : MAX_BUFFERS;
	mFinish = 0;
}

V4L2_VidCapManager::~V4L2_VidCapManager()
{
	clearBuffers();
}

int V4L2_VidCapManager::init()
{
	// initialize capture stuff

	int res = -1;

	// init the lock
	pthread_mutex_init(&mLock, 0);
	// default method
	mMethod = IO_METHOD_NOCAP;
	
	if(mDeviceDescriptor->isVideoCaptureDev()) {
		// device supports read/write
		if(mDeviceDescriptor->isRWDev())
			mMethod = IO_METHOD_READ;

		// device supports streaming IO with mmaped or user buffers
		if(mDeviceDescriptor->isStreamingDev()) {
			struct v4l2_requestbuffers req;
			memset(&req, 0, sizeof(req));
			
			// try to allocate a kernel buffer for mmap to determine method
			req.count	= 1;
			req.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
			req.memory	= V4L2_MEMORY_MMAP;

			// if successful we use mmap otherwise we use user pointer
			if(ioctl(mDeviceDescriptor->getHandle(), VIDIOC_REQBUFS, &req) != -1) {
			      mMethod = IO_METHOD_MMAP;
			      // and free the buffer on success
			      req.count	= 0;
			      ioctl(mDeviceDescriptor->getHandle(), VIDIOC_REQBUFS, &req);
			} else
				mMethod = IO_METHOD_USERPTR;
			
			
		}
	}

	// method dependend initialization	
	switch(mMethod)
	{
		case IO_METHOD_READ:
		break;
		
		case IO_METHOD_MMAP:
		break;

		case IO_METHOD_USERPTR:
		break;
	}
	
	return res;
}

int V4L2_VidCapManager::start_read()
{
	// start capture if read() IO method is used.
	int size = mFormatMgr->getImageSize();
	
	// allocate IOBuffers for the images
	for(int i = 0; i < mNumBufs; i++) {
		void *ptr = new char[size];
		
		IOBuffer *buf = new IOBuffer(this, ptr, size);
		mBuffers.push_back(buf);
	}

	// store the current time to create a propper time stamp
	gettimeofday(&mStartTime, 0);

	// get streaming parameters for the timing	
	struct v4l2_streamparm param;
	memset(&param, 0, sizeof(v4l2_streamparm));
	param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOC_G_PARM, &param) != -1) {
		mDTNumerator = param.parm.capture.timeperframe.numerator;
		mDTDenominator = param.parm.capture.timeperframe.denominator*10000000;
	} else {
		// the VIDIOC_G_PARM isn't supported by the driver so use the framerate of the video standard
		if(mFormatMgr->getVideoStandard() == 0) {
			mDTNumerator = 1;
			mDTDenominator = 25;
		} else {
			switch(mFormatMgr->getVideoStandard()->id)
			{
				case V4L2_STD_525_60:
					mDTNumerator = 1001;
					mDTDenominator = 30000;
				break;
		
				case V4L2_STD_625_50:
					mDTNumerator = 1;
					mDTDenominator = 25;
				break;
				
				default:
					mDTNumerator = 1;
					mDTDenominator = 25;
				break;			
			}
		} 
	}
	
	return 0;
}

int V4L2_VidCapManager::stop_read()
{
	return 0;
}

int V4L2_VidCapManager::start_mmap()
{
	// start cpturing for the mmap IO-method.

	// request the buffers
	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof(req));
			
	req.count	= mNumBufs;
	req.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory	= V4L2_MEMORY_MMAP;

	if(ioctl(mDeviceDescriptor->getHandle(), VIDIOC_REQBUFS, &req) == -1)
		return -1;
		
	if(req.count < 2)
		return -1;

	mNumBufs = req.count;
	
	// enumerate the buffers
	for(int i = 0; i < mNumBufs; i++) {
		struct v4l2_buffer	buf;
		memset(&buf, 0, sizeof(buf));
		
		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i;

		if(ioctl(mDeviceDescriptor->getHandle(), VIDIOC_QUERYBUF, &buf) == -1) {
			logDebug(std::string("V4L2_VidCapManager: QUERYBUF failed: ") + strerror(errno));
			return -1;
		}
		
		// and mmap them to the userspace
		void *start = mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, 
							mDeviceDescriptor->getHandle(), buf.m.offset);
							
		// mmap failed
		if(start == MAP_FAILED)	{
			logDebug(std::string("V4L2_VidCapManager: mmap failed: ") + strerror(errno));
			return -1;
		}

		// create an IOBuffer containing the mmaped buffer and store it in the buffer list
		IOBuffer *io_buf = new IOBuffer(this, start, buf.length, buf.index);		
		mBuffers.push_back(io_buf);
	}

	// enqueue the buffers into the incomming queue
	for(IOBufList::iterator it = mBuffers.begin(); it != mBuffers.end(); it++) {
		IOBuffer	*io_buf = *it;
		io_buf->setState(IOBuffer::STATE_USED);
		enqueue(io_buf);
	}
	
	// and start capturing
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == ioctl (mDeviceDescriptor->getHandle(), VIDIOC_STREAMON, &type)) {
		logDebug(std::string("V4L2_VidCapManager: STREAMON failed: ") + strerror(errno));
 		return -1;
 	}
 	
	return 0;
}

int V4L2_VidCapManager::stop_mmap()
{
int res = 0;

	// mmap-specific stop capture method.	
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == ioctl (mDeviceDescriptor->getHandle(), VIDIOC_STREAMOFF, &type))
		return -1;

	// and release all buffers
	clearBuffers();

	return res;
}

int V4L2_VidCapManager::start_userptr()
{
	
	return 0;
}

int V4L2_VidCapManager::stop_userptr()
{
	return 0;
}

int V4L2_VidCapManager::destroy()
{
	// cleanup stuff
	int res = 0;

	switch(mMethod)
	{
		case IO_METHOD_READ:
		break;
		
		case IO_METHOD_MMAP:
		break;

		case IO_METHOD_USERPTR:
		break;
	}

	// destroy mutex
	pthread_mutex_destroy(&mLock);
		
	return res;
}

void V4L2_VidCapManager::run(void* mgr)
{
	// capture main loop

	V4L2_VidCapManager* io_mgr = (V4L2_VidCapManager*) mgr;

	// run the loop until finish flag is set
	while(!io_mgr->mFinish) {
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
		if (-1 == res) {
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
		
		if(io_buf) {
			// test whether finish flag has been set in between
			if (!io_mgr->mFinish) {
				// and call the capture handler if one is registered
				if(io_mgr->getCaptureHandler()) {
					// std::cout<<"Calling capture-handler for #"<<io_buf->getSequence()<<"\n";
			 		io_mgr->getCaptureHandler()->handleCaptureEvent(io_buf);
				} else {
			 		io_mgr->enqueue(io_buf); // otherwise enqueue buffer directly
				}
			} else {
				io_mgr->enqueue(io_buf);
			}
		} else {
			// wait 5 ms to give clients the chance to free a buffer without polling on select
			usleep((int)(1000.0*5));
		}
	}
}

int V4L2_VidCapManager::startCapture()
{
int res = 0;

	// hack for Logitech QuickCam Pro 5000
	if(mDeviceDescriptor->getDriver() == "uvcvideo" && 
		mFormatMgr->getFormat() && mFormatMgr->getFormat()->getName() == "YUYV")
		mFormatMgr->setFramerate(30);
		
	mFormatMgr->flush();
	
	// is the capture thread already running?
	if(mThread != 0)
		return -1;
	
	// reset values
	mFinish = 0;	
	mBuffers.clear();
	mSequence = 0;
	
	// reset cropping params
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	
	memset (&cropcap, 0, sizeof (cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	// drivers should implement this ioctl, but not all do so
	if (-1 != ioctl (mDeviceDescriptor->getHandle(), VIDIOC_CROPCAP, &cropcap)) {
		// reset cropping only if it is supported
		memset (&crop, 0, sizeof (crop));
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; 
		
		// Ignore if cropping is not supported (EINVAL)
		if (-1 == ioctl (mDeviceDescriptor->getHandle(), VIDIOC_S_CROP, &crop) && errno != EINVAL) 
			return -1;
	}
	
	// call the IO-method-specific start-method
	switch(mMethod)
	{
		case IO_METHOD_READ:
			res = start_read();
		break;
		
		case IO_METHOD_MMAP:
			res = start_mmap();
		break;

		case IO_METHOD_USERPTR:
			res = start_userptr();
		break;
	}

	// create and start a new pthread
	mThread = new pthread_t; 
	pthread_create( mThread, 0,  (void* (*)(void*)) &V4L2_VidCapManager::run, (void*) this );

	return res;
}

int V4L2_VidCapManager::stopCapture()
{
int res = 0;
	   
	// Stops capturing

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

		case IO_METHOD_USERPTR:
			res = stop_userptr();
		break;
	}

	// iterate the buffer list and mark all buffers UNUSED
	for(IOBufList::iterator it = mBuffers.begin(); it != mBuffers.end(); it++)
	{
		IOBuffer *buf = *it;
		if(buf)
			(buf->setState(IOBuffer::STATE_UNUSED));
	}

	// and kick the buffers
	clearBuffers();
	
	return res;
}


int V4L2_VidCapManager::enqueue(IOBuffer *io_buf)
{
	// enqueues an IOBuffer into the queue of incomming buffers, 
	// enables the driver to reuse the buffer

	struct v4l2_buffer buf;
	int res = 0;
	
	// don't do anything, if already stopped
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
				
			// otherwise enqueue the buffer in the drivers incomming buffer queue
			memset(&buf, 0, sizeof(v4l2_buffer));
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = io_buf->getIndex();
			
			res = ioctl (mDeviceDescriptor->getHandle(), VIDIOC_QBUF, &buf);
			
			pthread_mutex_unlock(&mLock);
			
			if(res)
				return -1;
		break;

		// not implemented yet
		case IO_METHOD_USERPTR:
		break;
	}
	
	return 0;
}

// get the current IOBuffer

IOBuffer* V4L2_VidCapManager::dequeue()
{
struct v4l2_buffer buf;
IOBuffer *res = 0;
	
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
        	for(IOBufList::iterator it = mBuffers.begin(); it != mBuffers.end(); it++) {
        		res = *it;
        		// first fit
        		if(res->getState() == IOBuffer::STATE_UNUSED)
        			break;
        	}
        	
        	// did we find a buffer
        	if(res->getState() == IOBuffer::STATE_UNUSED) {
        		// then compute the timestamp from the start-time, the framerate and the sequence number
        		struct timeval	tv;

        		// the time between 2 frames is given as rational number
        		// so compute the elapsed time so far
        		double elapsed = ((double) mSequence) * ((double)mDTNumerator)/((double)mDTDenominator);
        		tv.tv_sec	= (int) trunc(elapsed);
        		tv.tv_usec	= (int) ((elapsed - trunc(elapsed)) * 1000000);
        		
        		// and add the start time
        		tv.tv_sec += mStartTime.tv_sec;
        		// did we have an overflow
        		if( tv.tv_usec + mStartTime.tv_usec >= 1000000) {
        			// then increase the seconds
        			tv.tv_sec++;
        			tv.tv_usec += mStartTime.tv_usec - 1000000;
        		} else {
	        		tv.tv_usec += mStartTime.tv_usec;
        		}
        		
	        	// read the captured data
				int n = read(mDeviceDescriptor->getHandle(), res->getPtr(), res->getSize());

				// and update the buffer parameter
				if(n > 0) {
					// std::cout<<"found empty buffer "<<res<<" State before: "<< res->getState();
					res->setParams(n, IOBuffer::STATE_USED, tv, mSequence++ + 1);
					mAvailableBuffers--;
					// std::cout<<" and after: "<<res->getState()<<"\n";
				} else {
					res = 0;
				}
        	} else {
				res = 0;
        	}
        	
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

			// just get the index of the current buffer from the driver
			memset(&buf, 0, sizeof(v4l2_buffer));
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			
			if (-1 == ioctl (mDeviceDescriptor->getHandle(), VIDIOC_DQBUF, &buf)) {
				pthread_mutex_unlock(&mLock);
				return 0;
			}
			
			// and find the corresponding buffer object
			res = findBuffer(buf.index);			
			
			if(buf.sequence == 0)
				buf.sequence = mSequence++;
			
			// set the buffer parameters
			if(res != 0) {
				res->setParams(buf.bytesused, IOBuffer::STATE_USED, buf.timestamp, buf.sequence + 1);
				mAvailableBuffers--;
			}
			pthread_mutex_unlock(&mLock);
			
		break;

		// not implemented yet
		case IO_METHOD_USERPTR:
		break;
	}
	
	return res;
}

IOBuffer* V4L2_VidCapManager::findBuffer(int index)
{
	// Find a buffer by its unique index

	IOBuffer	*io_buf = 0;
	
	// iterate the list and return the correct buffer object
	for(IOBufList::iterator it = mBuffers.begin(); it != mBuffers.end(); it++) {
		io_buf = *it;
		if(io_buf->getIndex() == index)
			break;
	}

	return io_buf;
}

int V4L2_VidCapManager::getUsedBufferCount()
{
	// Return the number of currently used buffers.

	int res = 0;

	// iterate the list and count the used buffers
	for(IOBufList::iterator it = mBuffers.begin(); it != mBuffers.end(); it++)
		if (*it)
			if((*it)->getState() == IOBuffer::STATE_USED)
				res++;
	
	return res;
}

void V4L2_VidCapManager::clearBuffers()
{
	// unmmaps and deletes all unused buffers.

	// synchronize access to the buffer list cause multiple threads can access them
	// when they release a buffer.
	pthread_mutex_lock(&mLock);

	// iterate the buffer list
	for(IOBufList::iterator it = mBuffers.begin(); it != mBuffers.end(); it++) {
		IOBuffer *buf = *it;
		if(buf) {
			if(buf->getState() == IOBuffer::STATE_UNUSED) {
				// the buffer isn't needed anymore
				switch(mMethod)
				{
					case IO_METHOD_READ:
						// so delete
						delete[] ((char*) buf->getPtr());
					break;
					
					case IO_METHOD_MMAP:
						// or munmap it
						munmap(buf->getPtr(), buf->getSize());
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

int V4L2_VidCapManager::getNumIOBuffers()
{
	return mAvailableBuffers;
}

