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

#include "FormatManager.h"
#include "DeviceDescriptor.h"

using namespace avcap;

// Construction & Destruction

FormatManager::FormatManager(DeviceDescriptor *dd):
	Manager<Format>(dd),
	mWidth(0),
	mHeight(0),
	mBytesPerLine(0), 
	mCurrentFormat(0), 
	mImageSize(0), 
	mModified(false)
{
}

FormatManager::~FormatManager()
{
	// delete the format objects
	for (ListType::iterator it = mFormats.begin(); it != mFormats.end(); it++)
		delete *it;

	// and the video standard objects
	for(VideoStandardList::iterator it = mStandards.begin(); it != mStandards.end(); it++)
		delete *it;
}

int FormatManager::setFormat(Format* f)
{
	if(f == 0)
		return -1;
	
#ifndef _WIN32
	// formats are identified by their forcc
	mCurrentFormat = f->getFourcc();
#endif
	
	// something has changed
	mModified = true;

	return 0;
}

int FormatManager::setFormat(uint32_t fourcc)
{
	for(ListType::const_iterator i = mFormats.begin(); i != mFormats.end(); i++) {
		Format	*f = *i;
		if(f->getFourcc() == fourcc)
			return setFormat(f);
	}
	
	return -1;
}

Format* FormatManager::getFormat()
{
Format *res = 0;

#ifndef _WIN32
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
#endif
	
	return res;
}

int FormatManager::setResolution(int w, int h)
{
	mWidth = w;
	mHeight = h;
	mModified = true;
	
	return flush();
}

int FormatManager::getWidth()
{
	if(flush() == -1)
		return 0;

	return mWidth;
}

int FormatManager::getHeight()
{
	if(flush() == -1)
		return 0;

	return mHeight;
}

int FormatManager::setBytesPerLine(int bpl)
{
	mBytesPerLine = bpl;
	mModified = true;
	
	return flush();
}

int FormatManager::getBytesPerLine()
{
	if(flush() == -1)
		return -1;

	return mBytesPerLine;
}

size_t FormatManager::getImageSize()
{
	if(flush() == -1)
		return 0;
		
	return mImageSize;	
}

int FormatManager::flush()
{
	// has something changed
	if(mModified) {
		mImageSize = mWidth*mHeight*3;
	}

	return 0;
}


const VideoStandard* FormatManager::getVideoStandard()
{			
	return 0;
}

int FormatManager::setVideoStandard(const VideoStandard* std)
{	
	return -1;
}

int FormatManager::setFramerate(int fps)
{
	return -1;
}
		
int FormatManager::getFramerate()
{
	return -1;
}

Format::~Format()
{
	// delete the resolution objects
	for(ResolutionList_t::iterator i = mResList.begin(); i != mResList.end(); i++)
		delete *i;
}

void Format::addResolution(int w, int h)
{
	mResList.push_back(new Resolution(w, h));
}
