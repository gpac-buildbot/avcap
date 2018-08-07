/*
 * (c) 2005 Nico Pranke <Nico.Pranke@googlemail.com>
 *
 * June, 2005: Win32 implementation by Robin Luedtke <RobinLu@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2. Refer
 * to the file "COPYING" for details.
 */

#ifndef TESTCAPTUREHANDLER_H_
#define TESTCAPTUREHANDLER_H_

#include <stdio.h>
#include <string>

#include "avcap/CaptureHandler.h"

namespace avcap
{
//! A simple CaptureHandler implementation.

/*! This is a simple implementation of a CaptureHandler. It just prints out
 * some informations about the frame and saves the data to a file if a filename is specified. */

class TestCaptureHandler:public CaptureHandler
{
private:
	FILE* mFile;

public:
	TestCaptureHandler(const std::string& filename = "");
	virtual ~TestCaptureHandler();

public:
	/* This method is called by the CaptureManager, when new data was captured.
	 * \param io_buf The buffer, that contains the captured data. */
	void handleCaptureEvent(IOBuffer* io_buf);
};
}

#endif //TESTCAPTUREHANDLER_H_
