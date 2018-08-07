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


#include "IOBuffer.h"

using namespace avcap;

// Construction & Destruction

IOBuffer::IOBuffer(CaptureManager* mgr, void *ptr, size_t size, int index)
		: mMgr(mgr), mPtr(ptr), mSize(size), mIndex(index), mSequence(0), mValid(0)
{
	mState = STATE_UNUSED;
	mTimestamp.tv_sec = 0;
	mTimestamp.tv_usec = 0;
}

IOBuffer::~IOBuffer()
{
}

void IOBuffer::setParams(const size_t v, State state, struct timeval &tv, int seq)
{
	mValid = v; 
	mState = state;
	mTimestamp.tv_sec = tv.tv_sec; 
	mTimestamp.tv_usec = tv.tv_usec;
	mSequence = seq; 
}

void IOBuffer::release()
{
	// enqueue the buffer in the capture-managers unused-buffer queue
	if(mMgr)
		mMgr->enqueue(this);
}

unsigned long IOBuffer::getTimestamp()
{ 
	unsigned long res = (unsigned long)((double) mTimestamp.tv_sec*1000.0f);
	res += (unsigned long) ((double) mTimestamp.tv_usec/1000.0f);

	return res; 
}


