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
#include <assert.h>

#include "HelpFunc.h"
#include "Crossbar.h"

#include "DS_ConnectorManager.h"
#include "DS_Connector.h"
#include "DS_DeviceDescriptor.h"
#include "log.h"

using namespace avcap;

// Construction & destruction

DS_ConnectorManager::DS_ConnectorManager(DS_DeviceDescriptor *dd) :
	ConnectorManager((DeviceDescriptor*)dd), mCrossbar(NULL),
			mDSDeviceDescriptor(dd)
{
	QzCComPtr<IGraphBuilder> FilterGraph;
	QzCComPtr<ICaptureGraphBuilder2> CaptureGraphBuilder;
	if (!GetFilterGraphFromFilter((IBaseFilter*)mDeviceDescriptor->getHandle(),
			&FilterGraph, &CaptureGraphBuilder)) {
		return;
	}

	mCrossbar=new CCrossbar(CaptureGraphBuilder);
	if (((CCrossbar*)mCrossbar)->FindAllCrossbarsAndConnectors((IBaseFilter*)mDSDeviceDescriptor->getHandle())
			<=0) {
		delete ((CCrossbar*)mCrossbar);
		mCrossbar=NULL;
	}
}

DS_ConnectorManager::~DS_ConnectorManager()
{
	if (mCrossbar!=NULL) {
		delete ((CCrossbar*)mCrossbar);
		mCrossbar=NULL;
	}
}

void DS_ConnectorManager::query()
{
	assert(mDSDeviceDescriptor != 0);
	int PinIndex=0;

	// Query video input connectors
	// Look for a crossbar in the filter graph
	if (mCrossbar!=NULL) {
		std::list<STConnector*> InputList=((CCrossbar*)mCrossbar)->GetInputConnectorList();

		for (std::list<STConnector*>::iterator Iter=InputList.begin(); Iter
				!=InputList.end(); Iter++) {
			long PhysicalType=0;
		
			if ((*Iter)->PhysicalType==PhysConn_Video_Tuner || (*Iter)->PhysicalType==PhysConn_Audio_Tuner) {
				
				// Check if this connector has a corresponding tuner
				if ((*Iter)->PhysicalType==PhysConn_Video_Tuner) {
			
					// Because of the video connector we expect a tv tuner
					// Look if the tuner has radio and/or tv tuner support
					int Capabilities=0;
					if (FindTunerRadioSupport(
							(IBaseFilter*)mDSDeviceDescriptor->getHandle(),
							&Capabilities)) {
				
						if (Capabilities & CAP_TUNER) {
							PhysicalType |= CAP_TUNER;
							PhysicalType |= DS_Connector::INPUT_TYPE_TUNER;
						}
					}
				}

				if ((*Iter)->PhysicalType==PhysConn_Audio_Tuner) {
					// Because of the audio connector we expect a radio tuner
					// Look if the tuner has radio and/or tv tuner support
					int Capabilities=0;
					if (FindTunerRadioSupport(
							(IBaseFilter*)mDSDeviceDescriptor->getHandle(),
							&Capabilities)) {
						
						if (Capabilities & CAP_RADIO) {
							PhysicalType |= CAP_RADIO;
							PhysicalType |= DS_Connector::INPUT_TYPE_TUNER;
						}
					}
				}
			}

			DS_Connector *con = new DS_Connector(mDSDeviceDescriptor,
					(int)(*Iter)->PinIndexOfCrossbar,
					(const std::string)(*Iter)->NameOfConnector,
					PhysicalType,
					(*Iter)->AudioSet);

			// Check if it is a video pin
			if ((*Iter)->IsVideoConnector) {
				mVideoInputs.push_back(con);
				logDebug("DS_ConnectorManager: (Crossbar) Video Input found: Name: " + 
						con->getName());
			}

			// Check if it is a audio pin
			if ((*Iter)->IsAudioConnector) {
				mAudioInputs.push_back(con);
				logDebug("DS_ConnectorManager: (Crossbar) Audio Input found: Name: "+con->getName());
			}
		}
	} else {	
		/* There is no crossbar present in the FilterGraph (only the CaptureFilter itself). Now this is a big problem
		 because we can't get all needed informations from the CaptureFilter itself. That means e.g. there is no chance 
		 to find out the real physical connectors or the association between video/audio connectors.
		 All new WDM capture devices with more than one input connector or tuner have crossbar support e.g. for switching between 
		 the connectors and for controlling the tuner. 
		 There are also new devices with WDM drivers (e.g. webcams) and no crossbar support. Why?
		 Some multimedia hardware (e.g. webcams) don't have multiple connectors or tuners. If it is the case the hardware vendors
		 have to create only a WDM/VFW(VFW is not used anymore) driver and they don't need to write any DirectShow capture filter or
		 crossbar filter etc., because DirectShow provides a WDM/VFW capture filter wrapper with no crossbar support. This wrapper 
		 filter has as many output pins as the hardware supports. The problem: it's impossible to find out the physical connector 
		 type of these pins. All informations available are the media format of each output pin. See the code below.
		 All old devices with VFW-drivers don't support crossbars. VFW devices can also have multiple connectors but they can 
		 be controlled only by VFW driver-supplied dialog boxes. */

		// Enumerate all pins on the capture filter
		std::list<IPin*> PinList;
		EnumPinsOnFilter((IBaseFilter*)mDSDeviceDescriptor->getHandle(),
				PinList);
		for (std::list<IPin*>::iterator Iter=PinList.begin(); Iter
				!=PinList.end(); Iter++) {
			PIN_INFO PinInfo;
			(*Iter)->QueryPinInfo(&PinInfo); // Get pin name
			
			// DirectShow documentation: "Every capture filter has at least one capture pin."
			// Look only for PIN_CATEGORY_CAPTURE pins
			GUID PinCategory;
			if (!getPinCategory((*Iter), &PinCategory)) {
				continue;
			} 
			
			// Get PinCategory
			if (PinCategory!=PIN_CATEGORY_CAPTURE) {
				continue;
			}
			
			// Enumerate all media types of the pin.
			std::list<AM_MEDIA_TYPE*> MediaTypeList;
			EnumMediaTypesOnPin((*Iter), MediaTypeList);
			for (std::list<AM_MEDIA_TYPE*>::iterator MediaTypeIter=
					MediaTypeList.begin(); MediaTypeIter!=MediaTypeList.end(); MediaTypeIter++) {
				char *PinName=(char*)WChar2Char(PinInfo.achName);
			
				if (PinName==NULL) {
					break;
				}
				
				DS_Connector *con = new DS_Connector(mDSDeviceDescriptor, PinIndex, (const char*)PinName);
				delete [] PinName;

				if ((*MediaTypeIter)->majortype==MEDIATYPE_Video) {
					mVideoInputs.push_back(con);
					PinIndex++;
					logDebug("DS_ConnectorManager: (No Crossbar) Video Input found: Name: " + 
							con->getName());
					break;
				}

				if ((*MediaTypeIter)->majortype==MEDIATYPE_Audio) {
					mAudioInputs.push_back(con);
					PinIndex++;
					logDebug("DS_ConnectorManager:(No Crossbar) Audio Input found: Name: " + 
							con->getName());
					break;
				}

				delete con;
			}
			DeleteList(MediaTypeList);
		}
		DeleteList(PinList);
	}
}

Connector* DS_ConnectorManager::findByIndex(const ListType& l, int index)
{
	// Find a connector by its unique index.
	Connector *res = 0;

	// Iterate the list	
	for (ListType::const_iterator it = l.begin(); it != l.end(); it++) {
		res = *it;
		if (res->getIndex() == index)
			return res;
	}

	return NULL;
}

Connector* DS_ConnectorManager::getVideoInput()
{
	// Return the current video input connector.
	assert(mDSDeviceDescriptor != 0);
	return getConnector(true);
}


int DS_ConnectorManager::setVideoInput(Connector* c)
{
	// Set the current video input.
	assert(mDSDeviceDescriptor != 0);
	
	if (c==NULL) {
		return -1;
	}

	return setConnector(true, c);
}

Connector* DS_ConnectorManager::getAudioInput()
{
	// Return the current audio input connector.
	assert(mDSDeviceDescriptor != 0);
	return getConnector(false);
}

int DS_ConnectorManager::setAudioInput(Connector* c)
{
	// Set the current audio input.
	assert(mDSDeviceDescriptor != 0);

	if (c==NULL) {
		return -1;
	}

	return setConnector(false, c);
}

Connector* DS_ConnectorManager::getVideoOutput()
{
	return 0;
}

int DS_ConnectorManager::setVideoOutput(Connector* c)
{
	return 0;
}

Connector* DS_ConnectorManager::getAudioOutput()
{
	return 0;
}

int DS_ConnectorManager::setAudioOutput(Connector* c)
{
	return 0;
}


int DS_ConnectorManager::setConnector(bool IsVideoConnector, Connector* c)
{
	int index = c->getIndex();

	// Look for a crossbar
	if (mCrossbar!=NULL) {
		// At least one crossbar is present
		
		if (((CCrossbar*)mCrossbar)->SetInput(index)) {
			return 1; // Successfully set
		}
	} else {
		//Get the corresponding IPin COM-Interface to the connector c
		IPin *ConnectorPin;
		getIPinFromConnector(c, &ConnectorPin,
				(IBaseFilter*)mDSDeviceDescriptor->getHandle());
		
		// Get currently connected audio input (it is a audio output pin on the capture filter)
		Connector *con;
		if (IsVideoConnector) {
			con=getVideoInput();
		} else {
			// Is audio connector
			con=getAudioInput();
		}
		
		// Look if the connector c is already connected
		if (c->getIndex()==con->getIndex()) {
			return 1;
		}
		
		/* Remove all following filter in the filtergraph starting from ConnectorPin which corresponds to 
		 the currently connected output pin on the capture filter */
		IPin *OutputPin=NULL;
		getIPinFromConnector(con, &OutputPin,
				(IBaseFilter*)mDSDeviceDescriptor->getHandle());
		
		if (OutputPin!=NULL) {
			if (!removeAllFilters(OutputPin)) {
				return 0;
			}
		}

		// Render the new selected output pin to the video rendering filter
		if (IsVideoConnector) {
			if (!RenderStream(ConnectorPin, true, false)) {
				return 0;
			}
		} else {
			// Is audio connector
			if (!RenderStream(ConnectorPin, false, true)) {
				return 0;
			}
		}
	}

	return 1;
}

Connector* DS_ConnectorManager::getConnector(bool IsVideoConnector)
{
	STConnector Con;
	std::list<Connector*> ConList;
	
	if (IsVideoConnector) {
		ConList=mVideoInputs;
	} else {
		ConList=mAudioInputs;
	}

	// Look for a crossbar
	if (mCrossbar!=NULL) // At least one crossbar is present
	{
		if (((CCrossbar*)mCrossbar)->GetCurrentVideoInput(&Con)) {
			return findByIndex(ConList, Con.PinIndexOfCrossbar);
		}
	} else {
		// No crossbar present - Look if the capture filter has a connected output pin.
		for (std::list<Connector*>::iterator Iter=ConList.begin(); Iter
				!=ConList.end(); Iter++) {
			// Check if the connector (output pin on capture filter) is connected to any other pin
			IPin *Pin=NULL;
			QzCComPtr<IPin> ConnectedPin;
			getIPinFromConnector((*Iter), &Pin,
					(IBaseFilter*)mDSDeviceDescriptor->getHandle());
			
			if (Pin==NULL) {
				continue;
			}
			
			if (SUCCEEDED(Pin->ConnectedTo(&ConnectedPin))) {
				if (Pin!=NULL) {
					Pin->Release();
				}
			
				ConnectedPin.Release();
				return (*Iter);
			}
			
			if (Pin!=NULL) {
				Pin->Release();
			}
			
			ConnectedPin.Release();
		}
	}

	return NULL;
}

void DS_ConnectorManager::getIPinFromConnector(Connector *con,
		IPin **ConnectorPin, IBaseFilter *CaptureFilter)
{
	// Enumerate all output pins on the capture filter
	std::list<IPin*> PinList;
	EnumPinsOnFilter(CaptureFilter, PinList);
	int PinIndex=0;
	
	for (std::list<IPin*>::iterator Iter=PinList.begin(); Iter!=PinList.end(); Iter++) {
		// DirectShow Documentation: "Every capture filter has at least one capture pin."
		// Look only for PIN_CATEGORY_CAPTURE pins
		GUID PinCategory;
		if (!getPinCategory((*Iter), &PinCategory)) {
			continue;
		} // Get PinCategory
	
		if (PinCategory!=PIN_CATEGORY_CAPTURE) {
			continue;
		}
		
		if (con->getIndex()==PinIndex) {
			(*Iter)->AddRef();
			*ConnectorPin=(*Iter);
			DeleteList(PinList);
			return;
		}
		PinIndex++;
	}
	DeleteList(PinList);
}

bool DS_ConnectorManager::removeAllFilters(IPin *OutputPinToStart)
{
	if (OutputPinToStart==NULL) {
		return false;
	}

	QzCComPtr<IPin> InputPin;
	InputPin=NULL;
	OutputPinToStart->ConnectedTo(&InputPin);

	if (FAILED(OutputPinToStart->Disconnect())) {
		return false;
	}

	while (InputPin!=NULL) {
		InputPin->Disconnect();
		PIN_INFO PinInfo;
	
		if (FAILED(InputPin->QueryPinInfo(&PinInfo))) {
			return false;
		}
		
		InputPin.Release();
		InputPin=NULL;

		// Find a downstream filter connected input pin
		std::list<IPin*> EnumPinList;
		EnumPinsOnFilter(PinInfo.pFilter, EnumPinList);
		
		for (std::list<IPin*>::iterator Iter=EnumPinList.begin(); Iter
				!=EnumPinList.end(); Iter++) {
			PIN_DIRECTION PinDir;
			(*Iter)->QueryDirection(&PinDir);
		
			if (PinDir==PINDIR_OUTPUT) {
				if (SUCCEEDED((*Iter)->ConnectedTo(&InputPin))) {
					break;
				} // We have found a connected pin
			}
		}
		DeleteList(EnumPinList);

		// Remove filter
		QzCComPtr<IGraphBuilder> FilterGraph;
		if (FAILED(GetFilterGraphFromFilter(PinInfo.pFilter, &FilterGraph))) {
			return false;
		}
		
		if (FAILED(FilterGraph->RemoveFilter(PinInfo.pFilter))) {
			return false;
		}
		FilterGraph.Release();
	}

	return true;
}

bool DS_ConnectorManager::getPinCategory(IPin *Pin, GUID *PinCategory)
{
	QzCComPtr<IKsPropertySet> Ks;
	if (FAILED(Pin->QueryInterface(IID_IKsPropertySet, (void **)&Ks))) {
		return false;
	}

	// Try to retrieve the pin category.
	DWORD cbReturned;
	if (FAILED(Ks->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0,
			PinCategory, sizeof(GUID), &cbReturned))) {
		return false;
	}

	return true;
}
