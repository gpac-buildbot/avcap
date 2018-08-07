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
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

#if !defined(_MSC_VER) && !defined(USE_PREBUILD_LIBS)
# include "avcap-config.h"
#endif

#include "DeviceCollector.h"
#include "log.h"

#ifdef AVCAP_LINUX
# include "V4L1_Device.h"
# include "V4L2_Device.h"
# include "V4L1_DeviceDescriptor.h"
# include "V4L2_DeviceDescriptor.h"
# ifdef HAS_AVC_SUPPORT
#  include "AVC_DeviceDescriptor.h"
#  include "AVC_Device.h"
#  include "raw1394util.h"
#  include <libavc1394/avc1394.h>
#  include <libavc1394/rom1394.h>
# endif
#endif

#ifdef AVCAP_OSX
# include "QT_Device.h"
# include "QT_DeviceDescriptor.h"
# include "QT_DeviceEnumerator.h"
#endif

#ifdef AVCAP_WINDOWS
# include "HelpFunc.h"
# include "DS_Device.h"
# include "DS_DeviceDescriptor.h"
#endif

using namespace avcap;

// Construction & Destruction
DeviceCollector::DeviceCollector()
{
	// query for available devices
#ifdef AVCAP_LINUX
	// prefer V4L2-driver over V4L1-driver, if both are available
	query_V4L2_Devices();
#ifndef AVCAP_HAVE_V4L2
	query_V4L1_Devices();
#endif
	query_ieee1394_Devices();
#endif

#ifdef AVCAP_OSX
	query_QT_Devices();
#endif

#ifdef AVCAP_WINDOWS
	// Init COM-library
	CoInitialize(NULL);
	query_DS_Devices();
#endif
}

DeviceCollector::~DeviceCollector()
{
	// delete all device descriptors
	for( DeviceList::iterator it = mDeviceList.begin(); it != mDeviceList.end(); it++) {
		delete *it;
	}

#ifdef AVCAP_OSX
	// QT cleanup stuff
	ExitMovies();
#endif

#ifdef AVCAP_WINDOWS
	// Free COM-library
	CoUninitialize();
#endif
}

// Linux-specific tests

#ifdef AVCAP_LINUX

void DeviceCollector::query_V4L2_Devices()
{
	// test all /dev/video* nodes, whether they are a valid V4L2 device.
	for (int i = 0; i < 64; i++) {
		std::ostringstream	ostr;
		ostr<<"/dev/video"<<i;
		test_V4L2_Device(ostr.str());
   	}
}

void DeviceCollector::query_V4L1_Devices()
{
#ifndef AVCAP_HAVE_V4L2
	// test all /dev/video* nodes, whether they are a valid V4L1 device.
	for (int i = 0; i < 64; i++) {
		std::ostringstream	ostr;
		ostr<<"/dev/video"<<i;
		test_V4L1_Device(ostr.str());
   	}
#endif
}

int DeviceCollector::test_V4L2_Device(const std::string& name)
{
	// Test whether the device is a V4L2 device and store the DeviceDescriptor if it is.
	// first check, if there already exists a descriptor for this dev-file
	for(DeviceList::const_iterator i = mDeviceList.begin(); i != mDeviceList.end(); i++)
		if((*i)->getName() == name)
			return 0;

	struct stat devstat;

	if ( stat(name.c_str(), &devstat) == 0)
	{
#ifdef DEBUG
		std::cout<<"DeviceCollector::queryDevices(): testing "<<name<<"...";
#endif
		// if it is not only a link
       	if (!S_ISLNK(devstat.st_mode))
       	{
       		// then create a V4L2-specific device descriptor
			V4L2_DeviceDescriptor *dd = new V4L2_DeviceDescriptor (name);

			// and test whether it is a valid V4L2 device
			if ( dd->isAVDev() )
			{
				// if so, store in the list
				mDeviceList.push_back(dd);
#ifdef DEBUG
				std::cout<<std::endl;
#endif
				return 0;
			}
			else
			{
#ifdef DEBUG
				std::cout<<" no Device.\n";
#endif
				// otherwise delete the device descriptor
				delete dd;
				return -1;
			}
		}
	}

	return -1;
}

int DeviceCollector::test_V4L1_Device(const std::string& name)
{
#ifndef AVCAP_HAVE_V4L2
	// first check, if there already exists a descriptor for this dev-file
	for(DeviceList::const_iterator i = mDeviceList.begin(); i != mDeviceList.end(); i++)
		if((*i)->getName() == name)
			return 0;

	struct stat devstat;

	if ( stat(name.c_str(), &devstat) == 0)
	{
#ifdef DEBUG
		std::cout<<"DeviceCollector::queryDevices(): testing "<<name<<"...";
#endif
		// if it is not only a link
       	if (!S_ISLNK(devstat.st_mode))
       	{
       		// then create a device descriptor for a V4L1-device
			V4L1_DeviceDescriptor *dd = new V4L1_DeviceDescriptor (name);

			// and test whether it is a valid V4L1 device
			if ( dd->isAVDev() )
			{
				// if so, store in the list
				mDeviceList.push_back(dd);
#ifdef DEBUG
				std::cout<<std::endl;
#endif
				return 0;
			}
			else
			{
#ifdef DEBUG
				std::cout<<" no Device.\n";
#endif
				// otherwise delete the device descriptor
				delete dd;
				return -1;
			}
		}
	}

#endif
	return -1;
}

void DeviceCollector::query_ieee1394_Devices()
{
#ifdef HAS_AVC_SUPPORT

	rom1394_directory rom_dir;
	raw1394handle_t handle;

	memset(&rom_dir, 0, sizeof(rom1394_directory));

	int device = -1;
	int i, j = 0;
	int m = raw1394_get_num_ports();

	for ( ; j < m && device == -1; j++ ) {
		handle = raw1394_open(j);

		for ( i = 0; i < raw1394_get_nodecount( handle ); ++i ) {

			// select AV/C Tape Reccorder Player node
			if ( rom1394_get_directory( handle, i, &rom_dir ) < 0 )	{
				logDebug("DeviceCollector: Error reading config rom directory for node: ", i );
				continue;
			}

			if ( ( rom1394_get_node_type( &rom_dir ) == ROM1394_NODE_TYPE_AVC ) &&
			        avc1394_check_subunit_type( handle, i, AVC1394_SUBUNIT_TYPE_VCR ) )
			{
				octlet_t guid = rom1394_get_guid( handle, i );
#ifdef DEBUG
				std::cout<<"DeviceCollector: Found AV/C device with GUID 0x"<<
					(quadlet_t) (guid>>32)<<((quadlet_t) (guid & 0xffffffff))<<"\n";
#endif

				device = i;

				AVC_DeviceDescriptor* dd = new AVC_DeviceDescriptor(guid);
				mDeviceList.push_back(dd);
			}
		}

		raw1394_close(handle);
	}
#endif
}

#endif

bool DeviceCollector::testDevice(const std::string& name)
{
#ifdef AVCAP_LINUX
	// prefer V4L2-driver
	if(test_V4L2_Device(name) == 0)
		return true;

	if(test_V4L1_Device(name) == 0)
		return true;
#endif

	return false;
}

// OS X specific tests

#ifdef AVCAP_OSX
void DeviceCollector::query_QT_Devices()
{
	// initialize QuickTime
	EnterMovies();

	// and enumerate capture-devices
	QT_DeviceEnumerator enumerator;
	if(enumerator.open() != 0)
		return;

	// add descriptors of the capture devices to the list
	enumerator.findDevices(mDeviceList);
}
#endif

// Windows-specific tests

#ifdef AVCAP_WINDOWS

// Query for capture devices using DirectShow.
void DeviceCollector::query_DS_Devices()
{
	// Find all installed video capture devices
	std::list<std::string> UniqueDeviceIDList;

	// Get the unique DeviceID string
	getInstalledDeviceIDs(UniqueDeviceIDList);

	// and test them
	for(std::list<std::string>::iterator i = UniqueDeviceIDList.begin();
		i != UniqueDeviceIDList.end(); i++)
		test_DS_Device(*i);
}

int DeviceCollector::test_DS_Device(const std::string& name)
{
	// Test whether the device is a capture device and store it in the device list if it is.
	logDebug("DS_DeviceCollector::test_DS_Device(): testing " + name + "...");

	for(DeviceList::const_iterator i = mDeviceList.begin(); i != mDeviceList.end(); i++)
		if((*i)->getName() == name)
			return 0;

	// Create a device descriptor
	DS_DeviceDescriptor *dd = NULL;
	dd = new DS_DeviceDescriptor (name);

	// Test whether we can make an instance of this capture filter. sometimes this isn't possible.
	if ( dd->isAVDev() )
	{
		// If so, store in the list
		mDeviceList.push_back(dd);
		return 0;
	}
	else
	{
		logDebug(" not a capture device.");

		// Otherwise delete the device descriptor
		delete dd;
		return -1;
	}

	return -1;
}

bool DeviceCollector::getInstalledDeviceIDs(std::list<std::string> &UniqueDeviceIDList)
{
	// Enumerate the video category
	// You can add other Category-CLSIDs to the list (e.g. audio category (gets audio capture filters))
	std::list<CLSID> EnumCategoriesList;
	EnumCategoriesList.push_back(CLSID_VideoInputDeviceCategory);
	//EnumCategoriesList.push_back(CLSID_AudioInputDeviceCategory);

	for(std::list<CLSID>::iterator EnumCategoriesListIter=EnumCategoriesList.begin(); EnumCategoriesListIter!=EnumCategoriesList.end(); EnumCategoriesListIter++)
	{
		// Create the system device enumerator.
		HRESULT hr;
		ICreateDevEnum *SysDevEnum = NULL;
		if(FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void **)&SysDevEnum))){return false;}

		// Obtain a class enumerator for the device capture category.
		IEnumMoniker *EnumCat = NULL;
		hr = SysDevEnum->CreateClassEnumerator(*EnumCategoriesListIter, &EnumCat, 0);

		if (hr == S_OK)
		{
			// Enumerate the monikers.
			IMoniker *Moniker = NULL;
			ULONG Fetched;
			while(EnumCat->Next(1, &Moniker, &Fetched) == S_OK)
			{
				IPropertyBag *PropBag;
				hr = Moniker->BindToStorage(0, 0, IID_IPropertyBag, (void **)&PropBag);
				if (SUCCEEDED(hr))
				{
					// The PropBag->Read(L"DevicePath"... gives a unique id but this property isn't available on all devices,
					// so we can't use it and have to create our own unique id.
					// We build a unique id-string in the form "<Name_of_Device><{CLSID_of_Device_Capture_DirectShow_Filter}>".
					VARIANT CLSIDString, FriendlyName;
					VariantInit(&CLSIDString); VariantInit(&FriendlyName);
					hr			= PropBag->Read(L"CLSID", &CLSIDString, NULL);
					HRESULT hr1 = PropBag->Read(L"FriendlyName", &FriendlyName, NULL);

					if(PropBag->Read(L"DevicePath", &CLSIDString, NULL)==S_OK)
					{
						UniqueDeviceIDList.push_back(bstr2string(CLSIDString.bstrVal));
					}
					else if (SUCCEEDED(hr) && SUCCEEDED(hr1))
					{
						UniqueDeviceIDList.push_back(bstr2string(FriendlyName.bstrVal)+bstr2string(CLSIDString.bstrVal));
					}
					VariantClear(&FriendlyName);
					VariantClear(&CLSIDString);

					// Remember to release filter later.
					PropBag->Release();
				}
				Moniker->Release();
			}
			EnumCat->Release();
		}
		SysDevEnum->Release();
	}

	return true;
}

#endif
