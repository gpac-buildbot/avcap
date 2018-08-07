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


#include <math.h>
#include <DShow.h>
#include <mtype.h> 
#include <string.h>

#include "HelpFunc.h"
#include "crossbar.h"

using namespace avcap;

CCrossbar::CCrossbar(ICaptureGraphBuilder2 *CaptureGraphBuilder) :
	m_CaptureBuilder(CaptureGraphBuilder) 
{
}

CCrossbar::~CCrossbar(void) 
{
	DeleteAllLists();
}

int CCrossbar::FindAllCrossbarsAndConnectors(IBaseFilter *StartFilter) 
{
	// The independent PinIndex is important, 
	//because there are connectors on different 
	// crossbars in a complex crossbar graph
	int PinIndex=0;
	StartFilter->AddRef();
	UINT FoundCrossbars=0;
	IAMCrossbar *Crossbar = NULL;
	QzCComPtr<IBaseFilter> TempStartFilter=StartFilter;

	// Find all crossbars in the filter graph
	while (m_CaptureBuilder->FindInterface(&LOOK_UPSTREAM_ONLY, NULL,
			TempStartFilter, IID_IAMCrossbar, (void**)&Crossbar)==S_OK) {
		TempStartFilter.Release();

		// We have found a crossbar - add it to the list
		m_CrossbarList.push_back(Crossbar);
		FoundCrossbars++;

		if (Crossbar->QueryInterface(IID_IBaseFilter, (void**)&TempStartFilter)
				!=S_OK) {
			break;
		}
	}

	// Find all seperate routings on all crossbars
	std::list<IAMCrossbar*>::iterator CrossbarIter;
	int CrossbarIndex=-1;
	for (CrossbarIter=m_CrossbarList.begin(); CrossbarIter
			!=m_CrossbarList.end(); CrossbarIter++) {
		CrossbarIndex++;
		IAMCrossbar *Crossbar=(*CrossbarIter);
		if (Crossbar==NULL) {
			return -1;
		}

		//Find all input pins on the crossbar
		long OutPinCount=0;
		long InPinCount=0;
		Crossbar->get_PinCounts(&OutPinCount, &InPinCount);

		for (long OutPinIndex=0; OutPinIndex<OutPinCount; OutPinIndex++) {
			bool FoundRoute=false;
			IPin *OutputPin=NULL;

			if (GetCrossbarIPinAtIndex(Crossbar, OutPinIndex, FALSE, &OutputPin)
					!=S_OK) {
				return -1;
			}

			for (long InPinIndex=0; InPinIndex<InPinCount; InPinIndex++) {
				long InPinPhysicalType=0;
				long OutPinPhysicalType=0;
				long PinRelated=0;

				// We can't use SUCCEEDED() macro - it's really strange
				// but if you use it - you will get true for every route
				if (Crossbar->CanRoute(OutPinIndex, InPinIndex) == S_OK) {
					FoundRoute=true;
					IPin *InputPin=NULL;

					if (GetCrossbarIPinAtIndex(Crossbar, InPinIndex, TRUE,
							&InputPin) != S_OK) {
						return -1;
					}

					STRouting *Routing=new STRouting();
					Routing->HasConnector=true; // We don't know if this routing has a connector - we set it to false
					Routing->Crossbar=Crossbar;
					Crossbar->AddRef();
					Routing->OutputPinIndex=OutPinIndex;
					Routing->OutputPin=OutputPin;
					OutputPin->AddRef();
					Routing->RelatedInputPin=InputPin;
					InputPin->AddRef();
					Routing->RelatedInPinIndex=InPinIndex;
					Routing->CrossbarIndex=CrossbarIndex;
					m_RoutingList.push_back(Routing);

					if (InputPin!=NULL) {
						InputPin->Release();
						InputPin=NULL;
					}
				}
			}
			
			if (OutputPin!=NULL) {
				OutputPin->Release();
				OutputPin=NULL;
			}
		}
	}

	// Find relation between multiple crossbars
	for (int CrossbarIndex=0; CrossbarIndex<(int)m_CrossbarList.size(); CrossbarIndex = +2) {
		for (std::list<STRouting*>::iterator RoutingIter1=m_RoutingList.begin(); RoutingIter1
				!=m_RoutingList.end(); RoutingIter1++) {
			if ((*RoutingIter1)->CrossbarIndex != CrossbarIndex) {
				continue;
			}

			for (std::list<STRouting*>::iterator RoutingIter2=
					m_RoutingList.begin(); RoutingIter2!=m_RoutingList.end(); RoutingIter2++) {
				if ((*RoutingIter2)->CrossbarIndex != CrossbarIndex+1) {
					continue;
				}

				QzCComPtr<IEnumMediaTypes> MediaTypeEnum;
				(*RoutingIter1)->RelatedInputPin->EnumMediaTypes(&MediaTypeEnum);
				AM_MEDIA_TYPE *MediaType=NULL;
				MediaTypeEnum->Next(1, &MediaType, NULL);
				HRESULT hr=(*RoutingIter2)->OutputPin->ReceiveConnection((*RoutingIter1)->RelatedInputPin, MediaType);

				if (hr==S_OK) {
					(*RoutingIter1)->HasConnector=false;
				}
				
				if ((*RoutingIter2)->OutputPin->QueryAccept(MediaType)==S_OK) {
					(*RoutingIter1)->HasConnector=false;
				}
				
				DeleteMediaType(MediaType);

			}
		}
	}

	// All routings with HasConnector==true are connectors and have to be added to the connector list
	for (std::list<STRouting*>::iterator RoutingIter=m_RoutingList.begin(); RoutingIter
			!=m_RoutingList.end(); RoutingIter++) {
		
		if ((*RoutingIter)->HasConnector) {
			STConnector *Connector = new STConnector;
			long PinRelated=0; // 0 means no related pin on the same crossbar
			(*RoutingIter)->Crossbar->get_CrossbarPinInfo(TRUE, (*RoutingIter)->RelatedInPinIndex, &PinRelated,
					&Connector->PhysicalType);
		
			// Get pin name
			StringFromPinType(Connector->NameOfConnector,
					Connector->PhysicalType);
			
			// Is it a video/audio pin
			if (Connector->PhysicalType < PhysConn_Audio_Tuner) {
				Connector->IsAudioConnector=false;
				Connector->IsVideoConnector=true;
			}
			
			if (Connector->PhysicalType >= PhysConn_Audio_Tuner) {
				Connector->IsAudioConnector=true;
				Connector->IsVideoConnector=false;
			}
			
			// Look if the routing is actual routed. important for getting the currently connected video input.
			long RoutedInputPin=0;
			Connector->IsRouted=false;
			HRESULT hr=(*RoutingIter)->Crossbar->get_IsRoutedTo((*RoutingIter)->OutputPinIndex, &RoutedInputPin);
			
			if (hr==S_OK) {
				if (RoutedInputPin==(*RoutingIter)->RelatedInPinIndex) {
					Connector->IsRouted=true;
				}
			}
			
			// Add crossbar and PinIndexOfCrossbar reference
			Connector->Crossbar = (*RoutingIter)->Crossbar;
			(*RoutingIter)->Crossbar->AddRef();
			Connector->PinIndexOfCrossbar = (*RoutingIter)->RelatedInPinIndex;
			Connector->AudioSet = (int)pow(2.0f, PinRelated);
			Connector->PinIndex=PinIndex;
			
			// Add input pin to the list
			m_InputConnectorList.push_back(Connector);
			PinIndex++; // Increment the independent PinIndex
		}
	}

	return FoundCrossbars;
}

void CCrossbar::DeleteAllLists() 
{
	// Delete RoutingList
	std::list<STRouting*>::iterator RoutingIter;
	for (RoutingIter=m_RoutingList.begin(); RoutingIter!=m_RoutingList.end(); RoutingIter++) {
		STRouting *Routing = *RoutingIter;

		if (Routing->Crossbar!=NULL) {
			Routing->Crossbar->Release();
			Routing->Crossbar=NULL;
		}
		
		if (Routing->OutputPin!=NULL) {
			Routing->OutputPin->Release();
		}
		
		if (Routing->RelatedInputPin!=NULL) {
			Routing->RelatedInputPin->Release();
		}
		
		delete Routing;
	}
	
	m_RoutingList.clear();

	// Delete CrossbarConnectorList
	std::list<IAMCrossbar*>::iterator CrossbarIter;
	for (CrossbarIter=m_CrossbarList.begin(); CrossbarIter
			!=m_CrossbarList.end(); CrossbarIter++) {
		IAMCrossbar *Crossbar=*CrossbarIter;
		if (Crossbar!=NULL) {
			Crossbar->Release();
		}
	}
	
	m_CrossbarList.clear();

	// Delete InputConnectorList
	std::list<STConnector*>::iterator InputConIter;
	for (InputConIter=m_InputConnectorList.begin(); InputConIter
			!=m_InputConnectorList.end(); InputConIter++) {
		STConnector *Connector = *InputConIter;
	
		if (Connector->Crossbar!=NULL) {
			Connector->Crossbar->Release();
		}
	
		delete Connector;
	}
	
	m_InputConnectorList.clear();
}

std::list<STConnector*>& CCrossbar::GetInputConnectorList() 
{
	return m_InputConnectorList;
}

bool CCrossbar::GetCurrentVideoInput(STConnector *Connector) 
{
	if (Connector==NULL) {
		return false;
	}

	for (std::list<STConnector*>::iterator InputConIter=
			m_InputConnectorList.begin(); InputConIter
			!=m_InputConnectorList.end(); InputConIter++) {
	
		if ((*InputConIter)->IsRouted && (*InputConIter)->IsVideoConnector) {
			memcpy(Connector, *InputConIter, sizeof(STConnector));
			return true;
		}
	}
	
	return false;
}

bool CCrossbar::GetCurrentAudioInput(STConnector *Connector) 
{
	if (Connector==NULL) {
		return false;
	}

	for (std::list<STConnector*>::iterator InputConIter=
			m_InputConnectorList.begin(); InputConIter
			!=m_InputConnectorList.end(); InputConIter++) {
		
		if ((*InputConIter)->IsRouted && (*InputConIter)->IsAudioConnector) {
			memcpy(Connector, *InputConIter, sizeof(STConnector));
			return true;
		}
	}
	
	return false;
}

bool CCrossbar::SetInput(int PinIndex) 
{
	STConnector *Connector=NULL;
	bool IsVideoConnector=false;
	bool IsAudioConnector=false;

	// Find input connector
	for (std::list<STConnector*>::iterator InputConIter=
			m_InputConnectorList.begin(); InputConIter
			!=m_InputConnectorList.end(); InputConIter++) {
		if ((*InputConIter)->PinIndex==PinIndex) {
			Connector=(*InputConIter);
		}
	}

	if (Connector==NULL) {
		return false;
	}

	for (std::list<STConnector*>::iterator InputConIter=
			m_InputConnectorList.begin(); InputConIter
			!=m_InputConnectorList.end(); InputConIter++) {
	
		if (((*InputConIter)->IsVideoConnector && Connector->IsVideoConnector)
				|| ((*InputConIter)->IsAudioConnector
						&& Connector->IsAudioConnector)) {
			(*InputConIter)->IsRouted=false;
		}
	}

	// Route the input connector to the corresponding output pin on the same crossbar
	bool RoutedSomething=false;
	IPin *OutputPin=NULL;
	for (std::list<STRouting*>::iterator RoutingIter=m_RoutingList.begin(); RoutingIter
			!=m_RoutingList.end(); RoutingIter++) {
		
		if ((Connector->Crossbar == (*RoutingIter)->Crossbar) && (Connector->PinIndexOfCrossbar == (*RoutingIter)->RelatedInPinIndex)) {
			if ((*RoutingIter)->Crossbar->Route((*RoutingIter)->OutputPinIndex, (*RoutingIter)->RelatedInPinIndex)!=S_OK) {
				return false;
			}
		
			RoutedSomething=true;
			OutputPin=(*RoutingIter)->OutputPin;
			Connector->IsRouted=true;
		}
	}

	// If multiple crossbars in the filter graph - find path through all chained crossbars
	bool CheckNextCrossbar=true;
	for (std::list<STRouting*>::iterator RoutingIter=m_RoutingList.begin(); RoutingIter
			!=m_RoutingList.end(); RoutingIter++) {
		
		STRouting *Routing=(*RoutingIter);
		while (CheckNextCrossbar) {
			// Get the input pin which is connected to the upstream crossbar output pin
			QzCComPtr<IPin> InputPin;
			if (OutputPin->ConnectedTo(&InputPin)!=S_OK) {
				break;
			}
		
			for (std::list<STRouting*>::iterator RoutingIter=
					m_RoutingList.begin(); RoutingIter!=m_RoutingList.end(); RoutingIter++) {
			
				// Look if there is a equal input pin
				if (InputPin==(*RoutingIter)->RelatedInputPin) {
					// For testing purpose
					PIN_INFO OutputPinName, InputPinName;
					InputPin->QueryPinInfo(&OutputPinName);
					OutputPin->QueryPinInfo(&InputPinName);

					if ((*RoutingIter)->Crossbar->Route((*RoutingIter)->OutputPinIndex, (*RoutingIter)->RelatedInPinIndex)!=S_OK) {
						return false;
					}
				
					RoutedSomething=true;
					OutputPin=(*RoutingIter)->OutputPin;
					CheckNextCrossbar=true; // Check next following crossbar for routing
					break;
				} else {
					CheckNextCrossbar=false;
				}
			}
		}
	}

	if (RoutedSomething) {
		return true;
	} else {
		return false;
	}
}

void CCrossbar::StringFromPinType(std::string &PinName, long lType) 
{
	switch (lType) {
	case PhysConn_Video_Tuner:
		PinName = "Video Tuner\0";
		break;
	case PhysConn_Video_Composite:
		PinName = "Video Composite\0";
		break;
	case PhysConn_Video_SVideo:
		PinName = "Video SVideo\0";
		break;
	case PhysConn_Video_RGB:
		PinName = "Video RGB\0";
		break;
	case PhysConn_Video_YRYBY:
		PinName = "Video YRYBY\0";
		break;
	case PhysConn_Video_SerialDigital:
		PinName = "Video SerialDigital\0";
		break;
	case PhysConn_Video_ParallelDigital:
		PinName = "Video ParallelDigital\0";
		break;
	case PhysConn_Video_SCSI:
		PinName = "Video SCSI\0";
		break;
	case PhysConn_Video_AUX:
		PinName = "Video AUX\0";
		break;
	case PhysConn_Video_1394:
		PinName = "Video 1394\0";
		break;
	case PhysConn_Video_USB:
		PinName = "Video USB\0";
		break;
	case PhysConn_Video_VideoDecoder:
		PinName = "Video Decoder\0";
		break;
	case PhysConn_Video_VideoEncoder:
		PinName = "Video Encoder\0";
		break;
	case PhysConn_Video_SCART:
		PinName = "Video SCART\0";
		break;
	case PhysConn_Video_Black:
		PinName = "Video Black\0";
		break;

	case PhysConn_Audio_Tuner:
		PinName = "Audio Tuner\0";
		break;
	case PhysConn_Audio_Line:
		PinName = "Audio Line\0";
		break;
	case PhysConn_Audio_Mic:
		PinName = "Audio Mic\0";
		break;
	case PhysConn_Audio_AESDigital:
		PinName = "Audio AESDigital\0";
		break;
	case PhysConn_Audio_SPDIFDigital:
		PinName = "Audio SPDIFDigital\0";
		break;
	case PhysConn_Audio_SCSI:
		PinName = "Audio SCSI\0";
		break;
	case PhysConn_Audio_AUX:
		PinName = "Audio AUX\0";
		break;
	case PhysConn_Audio_1394:
		PinName = "Audio 1394\0";
		break;
	case PhysConn_Audio_USB:
		PinName = "Audio USB\0";
		break;
	case PhysConn_Audio_AudioDecoder:
		PinName = "Audio Decoder\0";
		break;

	default:
		PinName = "Unknown\0";
		break;
	}
}

HRESULT CCrossbar::GetCrossbarIPinAtIndex(IAMCrossbar *pXbar, LONG PinIndex,
		BOOL IsInputPin, IPin ** ppPin) 
{
	LONG cntInPins, cntOutPins;
	IPin *pP = 0;
	IBaseFilter *pFilter = NULL;
	IEnumPins *pins=0;
	ULONG n;
	HRESULT hr;

	if (!pXbar || !ppPin)
		return E_POINTER;

	*ppPin = 0;

	if (S_OK != pXbar->get_PinCounts(&cntOutPins, &cntInPins))
		return E_FAIL;

	LONG TrueIndex = IsInputPin ? PinIndex : PinIndex + cntInPins;

	hr = pXbar->QueryInterface(IID_IBaseFilter, (void **)&pFilter);

	if (hr == S_OK) {
		if (pFilter->EnumPins(&pins)==S_OK) {
			LONG i=0;
			while (pins->Next(1, &pP, &n) == S_OK) {
				if (i == TrueIndex) {
					*ppPin = pP;
					break;
				}
				pP->Release();
				i++;
			}
			pins->Release();
		}
		pFilter->Release();
	}

	return *ppPin ? S_OK : E_FAIL;
}
