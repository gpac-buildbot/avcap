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
#include <time.h>
#include <string.h>

#include "log.h"
#include "QT_VidCapManager.h"
#include "QT_DeviceDescriptor.h"
#include "QT_FormatManager.h"
#include "IOBuffer.h"
#include "CaptureHandler.h"

using namespace avcap;

// Construction & Destruction

QT_VidCapManager::QT_VidCapManager(QT_DeviceDescriptor* dd, QT_FormatManager *fmt_mgr, int nbufs):
	mQTDeviceDescriptor(dd), 
	mFormatMgr(fmt_mgr),
	mThread(0),
	mFinish(0),
	mSequence(0),
	mNumBufs(nbufs),
	mAvailableBuffers(0),
	mDecompSession(0)
{	
}

QT_VidCapManager::~QT_VidCapManager()
{
	clearBuffers();
}

int QT_VidCapManager::init()
{
	// init the lock
	pthread_mutex_init(&mLock, 0);

	return 0;
}

int QT_VidCapManager::destroy()
{	
	stopCapture();
	
	// destroy mutex
	pthread_mutex_destroy(&mLock);

	return 0;
}

int QT_VidCapManager::startCapture()
{
	if(startCaptureImpl() == -1) {
		logDebug("QT_VidCapManager: failed to start capture.");
	}

	mFinish = false;
	mThread = new pthread_t; 
	pthread_create( mThread, 0,  (void* (*)(void*)) &QT_VidCapManager::threadFunc, (void*) this );

	return 0;
}

int QT_VidCapManager::startCaptureImpl()
{
	clearBuffers();

	OSErr err;

	// initialize and start the grabber component
	if((err = SGSetChannelUsage(mQTDeviceDescriptor->getChannel(), seqGrabRecord))) {
		logDebug("QT_VidCapManager: SGSetChannelUsage failed", err);
		return -1;
	}

	if((err = SGSetDataProc(mQTDeviceDescriptor->getGrabber(), QT_VidCapManager::captureCallback, (long) this))) {
		logDebug("QT_VideCapManager: SGSetDataProc failed", err);
		return -1;
	}

	if((err = SGStartRecord(mQTDeviceDescriptor->getGrabber()))) {
		logDebug("QT_VidCapManager: SGStartRecord failed", err);
		return -1;
	}

	if((err = SGGetChannelTimeScale(mQTDeviceDescriptor->getChannel(), &mTimeScale)))	{
		logDebug("QT_VidCapManager: SGGetChannelTimeScale() failed", err);
	}
	
	size_t img_size = mFormatMgr->getImageSize();
	if(!img_size) {
		logDebug("QT_VidCapManager: failed to determine size of captured images.");
	}
	
	// allocate the buffers, mark them unused and enqueue them
	for(int i = 0; i < mNumBufs; i++) {
		uint8_t* mem  = new uint8_t[img_size]; 
		IOBuffer* buf = new IOBuffer(this, mem, img_size, i);
		mBuffers.push_back(buf);
		buf->setState(IOBuffer::STATE_USED);
		enqueue(buf);
	}
	
	mSequence = 0;
	mLastTime = 0;

	return 0;
}


int QT_VidCapManager::stopCapture()
{	
	if(mThread) {
		stopCaptureImpl();
		
		// wait for the right moment to set the flag
		pthread_mutex_lock(&mLock);
		mFinish = true;
		pthread_mutex_unlock(&mLock);

		// wait until capture thread has finished and delete it
		pthread_join( *mThread, 0);
		delete mThread;
		mThread = 0;
				
		// dispose buffers
		clearBuffers();
		
		if(mDecompSession) {
			ICMDecompressionSessionFlush(mDecompSession);
			ICMDecompressionSessionRelease(mDecompSession);
			mDecompSession = 0;
		}
	}
	
	return 0;
}

int QT_VidCapManager::stopCaptureImpl()
{
	OSErr err;
	
	// stop the SequenceGrabber
	if((err = SGStop(mQTDeviceDescriptor->getGrabber()))) {
		logDebug("QT_VidCapManager: SGStop failed", err);
		return -1;
	}
	
	return 0;
}


void* QT_VidCapManager::threadFunc(void* arg)
{
	QT_VidCapManager* vcm_obj = (QT_VidCapManager*) arg;
	vcm_obj->threadFuncImpl();
	return 0;
}

struct timespec QT_VidCapManager::getPollTimespec(double fps)
{
	const double fudge_factor = 0.5;
	const long ns_per_ms = 1000000;
	const double ns_per_s = 1000000000.0;
	const long wait_ns = (long)(fudge_factor * ns_per_s / fps);

	const long min_wait_ns = 5 * ns_per_ms;
	const long max_wait_ns = 10 * ns_per_ms;

	struct timespec ts;

	ts.tv_sec = 0;
	ts.tv_nsec = wait_ns < min_wait_ns ?
		min_wait_ns :
		(wait_ns > max_wait_ns ? max_wait_ns :
		 wait_ns);

//	std::cout<<"getPollTimespec: fps = "<<fps<<" wait: "<<ts.tv_nsec<<"\n";
	
	return ts;
}

void QT_VidCapManager::threadFuncImpl(void)
{
	EnterMoviesOnThread(0);

	struct timespec wts = getPollTimespec(mFormatMgr->getFramerate());
	while(!mFinish) {
		struct timespec rts;

		if(poll()) {
			mFinish = true;
		}

		if(nanosleep(&wts, &rts) == -1) {
			if(errno != -EINTR ) {
				logDebug("QT_VidCapManager: nanosleep() failed", errno);
				mFinish = true;
			}
		}
	} 
	
	ExitMoviesOnThread();	
}

OSErr QT_VidCapManager::captureCallback(SGChannel ch, Ptr data, long data_len, 
	long * offset, long ch_ref_con, TimeValue time_value,
	short write_type, long priv)
{
	QT_VidCapManager* vcm_obj = (QT_VidCapManager*) priv;
	vcm_obj->captureCallbackImpl(ch, data, data_len, offset, ch_ref_con, time_value, write_type);
	
	return noErr;
}

OSErr QT_VidCapManager::captureCallbackImpl(SGChannel ch, Ptr data, long data_len, long * offset, 
			long ch_ref_con, TimeValue time_value, short write_type)
{
//	std::cout<<"captureCallback: "<<time_value<<", "<<data_len<<std::endl;	
	mSequence++;
	
	if(mFormatMgr->needsDecompression())
		decompressData(data, data_len, time_value);
	else {
		notifyCaptureHandler(data, data_len, 0, time_value);
	}
			
	return noErr;
}

// enqueues an IOBuffer into the queue of incomming buffers, e.g. enables the driver to use the buffer

int QT_VidCapManager::enqueue(IOBuffer *io_buf)
{
	pthread_mutex_lock(&mLock);
	
	if(mFinish) {
		// delete the buffer if capture has already finished
		delete[] ((uint8_t*) io_buf->getPtr());
		delete io_buf;
	} else {
		io_buf->setState(IOBuffer::STATE_UNUSED);
		mAvailableBuffers++;
		pthread_mutex_unlock(&mLock);
	}
	
	return 0;
}

IOBuffer* QT_VidCapManager::dequeue()
{
	// get the current IOBuffer
	IOBuffer* res = 0;
	
	pthread_mutex_lock(&mLock);
	
	// find the first unused buffer
    for(IOBufList_t::iterator it = mBuffers.begin(); it != mBuffers.end(); it++) {
        // find first unused buffer
        if((*it) && (*it)->getState() == IOBuffer::STATE_UNUSED) {
        	res = *it;
			break;
		}
	}
	
    // set its state to used
	if(res) {
		res->setState(IOBuffer::STATE_USED);
		mAvailableBuffers--;
	}
	
	pthread_mutex_unlock(&mLock);
	return res;
}


int QT_VidCapManager::poll()
{
	OSErr err;

	// SGIdle actually performs capturing
	if((err = SGIdle(mQTDeviceDescriptor->getGrabber())))	{
		if ( err == -2211 )	{
			logDebug("QT_VidCapManager: device removed during call to SGIdle()");
			return -1;
		}

		logDebug("QT_VidCapManager: SGIdle failed", err);
		return -1;
	}

	return 0;
}

int QT_VidCapManager::createDecompSession()
{
	// the decompression process is described by a decompression-session

	Rect r;
	OSErr err;
	
	if(noErr != (err = SGGetSrcVideoBounds(mQTDeviceDescriptor->getChannel(), &r))) {
		logDebug("QT_VidCapManager: SGGetSrcVideoBounds failed", err);
		return err;
	}

	ICMDecompressionTrackingCallbackRecord trackingCallbackRecord;
	ICMDecompressionSessionOptionsRef sessionOptions = 0;
	SInt32 displayWidth, displayHeight;

	ImageDescriptionHandle imageDesc = (ImageDescriptionHandle) NewHandle(0);        
	if(noErr != (err = SGGetChannelSampleDescription(mQTDeviceDescriptor->getChannel(), (Handle)imageDesc))) {
		DisposeHandle((Handle)imageDesc);
		logDebug("QT_VidCapManager: SGGetChannelSampleDescription failed", err);
		return err;
	}
	
	// assign a tracking callback
	trackingCallbackRecord.decompressionTrackingCallback = &QT_VidCapManager::decompTrackingCallback;
	trackingCallbackRecord.decompressionTrackingRefCon = this;
	
	// we also need to create a ICMDecompressionSessionOptionsRef to fill in codec quality
	err = ICMDecompressionSessionOptionsCreate(0, &sessionOptions);
	if (err == noErr) {
		CodecQ quality = 1023;
		ICMDecompressionSessionOptionsSetProperty(sessionOptions,
				kQTPropertyClass_ICMDecompressionSessionOptions,
				kICMDecompressionSessionOptionsPropertyID_Accuracy,
				sizeof(CodecQ), &quality);
	}
  
	// set desired properties of the decoded image
	CFMutableDictionaryRef pixelBufferAttribs = 
		CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
			&kCFTypeDictionaryKeyCallBacks,	&kCFTypeDictionaryValueCallBacks); 
			
	if(!pixelBufferAttribs) {
		logDebug("QT_VidCapManager: Unable to create CFMutableDictionary");
	} else {
		// width
		SInt32 width = mFormatMgr->getWidth();
		CFNumberRef width_n = CFNumberCreate(0, kCFNumberSInt32Type, &width);
		CFDictionarySetValue(pixelBufferAttribs, kCVPixelBufferWidthKey, width_n);
		CFRelease(width_n);

		//height
		SInt32 height = mFormatMgr->getHeight();
		CFNumberRef height_n = CFNumberCreate(0, kCFNumberSInt32Type, &height);
		CFDictionarySetValue(pixelBufferAttribs, kCVPixelBufferHeightKey, height_n);
		CFRelease(height_n);

		// pixel-format
		SInt32 pixel_fmt = QT_FormatManager::Fourcc2OSType(mFormatMgr->getFormat()->getFourcc());
		CFNumberRef pixel_fmt_n = CFNumberCreate(0, kCFNumberSInt32Type, &pixel_fmt);
		CFDictionarySetValue(pixelBufferAttribs, kCVPixelBufferPixelFormatTypeKey, pixel_fmt_n);
		CFRelease(pixel_fmt_n);
		
		CFDictionarySetValue(pixelBufferAttribs, kCVPixelBufferOpenGLCompatibilityKey, kCFBooleanTrue);
		
		// now make a new decompression session to decode source video frames to pixel buffers
		err = ICMDecompressionSessionCreate(NULL, imageDesc, sessionOptions, 
					(CFDictionaryRef) pixelBufferAttribs, &trackingCallbackRecord, &mDecompSession);
	
		CFRelease(pixelBufferAttribs);
		ICMDecompressionSessionOptionsRelease(sessionOptions);
		DisposeHandle((Handle)imageDesc);
	}

	return 0;
}

int QT_VidCapManager::decompressData(void* data, long length, TimeValue timeValue)
{
    OSStatus err = noErr;
    ICMFrameTimeRecord frameTime = {{0}};
    
    // Make a decompression session!!
    if(!mDecompSession) {
		if(createDecompSession() != 0) {
			logDebug("QT_VidCapManager: failed to create decompress session.");
			return -1;
		}
	}
    
    frameTime.recordSize = sizeof(ICMFrameTimeRecord);
    *(TimeValue64*)&frameTime.value = timeValue;
    frameTime.scale = mTimeScale;
    frameTime.rate = fixed1;
    frameTime.frameNumber = mSequence;
    frameTime.flags = icmFrameTimeIsNonScheduledDisplayTime;
    
    // push the frame into the session
    err = ICMDecompressionSessionDecodeFrame( mDecompSession, (UInt8 *)data, length, NULL, &frameTime, this);
    
    // and suck it back out
    ICMDecompressionSessionSetNonScheduledDisplayTime( mDecompSession, timeValue, mTimeScale, 0 );

    mLastTime = timeValue;
	
	return 0;
}

void QT_VidCapManager::notifyCaptureHandler(void* data, size_t length, size_t bytes_per_row, TimeValue time)
{
	IOBuffer* io_buf = dequeue();

	if(!io_buf) {
		logDebug("QT_VidCapManager: no free IOBuffer found.");
		return;
	}
	
	if(mFormatMgr->needsDecompression() && mFormatMgr->getFormat()->getFourcc() == PIX_FMT_YUYV) {
		// this means we have padding at the end of lines, which is not supported
		// by the IOBuffer. So we have to copy each single line
		if(io_buf->getSize() < mFormatMgr->getWidth() * mFormatMgr->getHeight() * 2) {
			logDebug("QT_VidCapManager: IOBuffer size to small", length);
			enqueue(io_buf);
			return;
		}

		// adjust src- and dst-pointers
		uint8_t* src = (uint8_t*) data;
		uint8_t* dst = (uint8_t*) io_buf->getPtr();
		size_t src_stride = bytes_per_row;
		size_t dst_stride = mFormatMgr->getWidth() * 2;
		
		// and copy line-wise
		for(int i = 0; i < mFormatMgr->getHeight(); i++, dst += dst_stride, src += src_stride) {
			memcpy(dst, src, dst_stride);
		}
	} else {
		// oops, something went wrong
		if(io_buf->getSize() < length) {
			logDebug("QT_VidCapManager: IOBuffer size to small", length);
			enqueue(io_buf);
			return;
		}
		
		// copy the frame-data to the buffer
		memcpy(io_buf->getPtr(), data, length);
	}
	
	// set timestamp and sequence-nr
	struct timeval tv;
	long msec = (double) time / (double) mTimeScale * 1000.0;
	tv.tv_sec = msec/1000;
	tv.tv_usec = (msec - tv.tv_sec * 1000) * 1000;
		
	io_buf->setParams(std::min(length, io_buf->getSize()), IOBuffer::STATE_USED, tv, mSequence);
	
	// and finaly call the capture-handler
	if(!mFinish && getCaptureHandler()) {
		getCaptureHandler()->handleCaptureEvent(io_buf);
	}
}


void QT_VidCapManager::decompTrackingCallback(void *decompressionTrackingRefCon, OSStatus result,
		ICMDecompressionTrackingFlags decompressionTrackingFlags, CVPixelBufferRef pixelBuffer,
		TimeValue64 displayTime, TimeValue64 displayDuration, ICMValidTimeFlags validTimeFlags,
		void *reserved,	void *sourceFrameRefCon )
{
	// The tracking callback function for the decompression session.
	// This method is called, if the decompression has finished. 
	
	QT_VidCapManager* vcm_obj = (QT_VidCapManager*) decompressionTrackingRefCon;
	
	if(result != noErr) {
		logDebug("QT_VidCapManager: decompression error", result);
		return;
	}

	OSType pixel_format = CVPixelBufferGetPixelFormatType(pixelBuffer);
	OSErr err;
	
	if((err = CVPixelBufferLockBaseAddress(pixelBuffer, 0))) {
		logDebug("QT_VidCapManager: CVPixelBufferLockBaseAddress failed", err);
		return;
	}
	
	// call the capture-handler with the decoded data
	vcm_obj->notifyCaptureHandler(
		CVPixelBufferGetBaseAddress(pixelBuffer),
		CVPixelBufferGetDataSize(pixelBuffer),
		CVPixelBufferGetBytesPerRow(pixelBuffer),
		displayTime);
		
	if((err = CVPixelBufferUnlockBaseAddress(pixelBuffer, 0))) {
		logDebug("QT_VidCapManager: CVPixelBufferUnlockBaseAddress failed", err);
		return;
	}
}


void QT_VidCapManager::clearBuffers()
{
	// synchronize access to the buffer list cause multiple threads can access them
	// when they release a buffer.
	pthread_mutex_lock(&mLock);

	// iterate the buffer list
	for(IOBufList_t::iterator it = mBuffers.begin(); it != mBuffers.end(); it++) {
		IOBuffer *buf = *it;
		if(buf && buf->getState() == IOBuffer::STATE_UNUSED) {
			delete[] ((uint8_t*) buf->getPtr());
				
			// and delete the buffer
			delete buf;
			*it = 0;
		}
	}

	int used = 0;

	// iterate the list and count the used buffers
	for(IOBufList_t::iterator it = mBuffers.begin(); it != mBuffers.end(); it++)
		if ((*it) && (*it)->getState() == IOBuffer::STATE_USED)
			used++;

	// if no buffer is there anymore clear the list
	if(used == 0)
		mBuffers.clear();
	
	// and unlock
	pthread_mutex_unlock(&mLock);
}

int QT_VidCapManager::getNumIOBuffers()
{
	return mAvailableBuffers;
}
