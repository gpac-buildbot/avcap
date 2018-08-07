/*
 * (c) 2005 Nico Pranke <Nico.Pranke@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2. Refer
 * to the file "COPYING" for details.
 */

#include <iostream>

#include "avcap/IOBuffer.h"

#include "TestCaptureHandler.h"

using namespace avcap;

// Construction & Destruction

TestCaptureHandler::TestCaptureHandler(const std::string& filename)
{
	if(filename != "")
		mFile = fopen(filename.c_str(), "wb");
	else
		mFile = 0;
}

TestCaptureHandler::~TestCaptureHandler()
{
	if(mFile)
		fclose(mFile);
}

void TestCaptureHandler::handleCaptureEvent(IOBuffer* io_buf)
{

	unsigned long tv = io_buf->getTimestamp();

	std::cout<<"\rCaptured frame: "<<io_buf->getSequence()<<" Timestamp: "<<tv<<
		" ms, Valid Bytes: "<<io_buf->getValidBytes();

	std::cout.flush();

	if(mFile && io_buf->getValidBytes() > 0)
		if(!fwrite(io_buf->getPtr(), io_buf->getValidBytes(), 1, mFile))
			std::cerr<<"TestCaptureHandler: Error writing data.\n";

	// and don't forget to release the buffer
	io_buf->release();
}


