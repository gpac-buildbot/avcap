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


#include "Qedit.h"

#include "HelpFunc.h"
#include "crossbar.h"
#include "DS_DeviceDescriptor.h"
#include "DS_Device.h"
#include "log.h"

using namespace avcap;

// Construction & Destruction

DS_DeviceDescriptor::DS_DeviceDescriptor(const std::string &card) :
	mCard(card), mCapabilities(0), mHandle(0), mValid(false), mIsOpen(false), mDevice(0)
{
	// test whether it is a valid device and query its capabilities
	mValid = queryCapabilities();
}

DS_DeviceDescriptor::~DS_DeviceDescriptor()
{
#ifdef DEBUG
	removeFromRot(mRegister);
#endif

	// Release the filter graph
	IGraphBuilder *FilterGraph=NULL;
	if (GetFilterGraphFromFilter((IBaseFilter*)mHandle, &FilterGraph)) {
		if (FilterGraph!=NULL) {
			FilterGraph->Release();
		}
	}

	// Release the capture filter
	if (((IBaseFilter*)mHandle)!=NULL) {
		((IBaseFilter*)mHandle)->Release();
		mHandle=NULL;
	}
}

CaptureDevice* DS_DeviceDescriptor::getDevice()
{
	if(!mDevice)
		mDevice = new DS_Device(this);

	return mDevice;
}

int DS_DeviceDescriptor::open()
{
	if(mIsOpen)
		return -1;

	// Not used
	return 0;
}

int DS_DeviceDescriptor::close()
{
	mIsOpen = 0;

	delete mDevice;
	mDevice = 0;

	// Not used
	return 0;
}

// Test and query device capabilities. return true, if video/audio device and false, if not.

bool DS_DeviceDescriptor::queryCapabilities()
{
	// Find the capture filter and get some infos about the device
	findDevice(mCard, (IBaseFilter **)&mHandle, mName, mInfo);
	if (mHandle==NULL) {
		return false;
	}

	// First build a complete filter graph
	if (!createCaptureFilterGraph(mCard, (IBaseFilter *)mHandle)) {
		return false;
	}

#ifdef DEBUG
	// Make the filter graph spyable (by GraphEdit software)
	QzCComPtr<IGraphBuilder> FilterGraph;
	if(!GetFilterGraphFromFilter((IBaseFilter*)mHandle, &FilterGraph))
	return false;

	addToRot(FilterGraph, &mRegister);
#endif

	mCapabilities=0;
	if (!isAudioOrVideoDevice((IBaseFilter *)mHandle, &mCapabilities))
		return false;
	if (!findOverlaySupport((IBaseFilter *)mHandle, &mCapabilities))
		return false;
	if (!findVBISupport((IBaseFilter *)mHandle, &mCapabilities))
		return false;
	if (!FindTunerRadioSupport((IBaseFilter *)mHandle, &mCapabilities))
		return false;

	logDebug("DS_DeviceDescriptor: Device found, name: "+ mName +
			" card: " + mCard +
			" bus: " + mInfo +
			" version: " + getVersionString());

	return true;
}

bool DS_DeviceDescriptor::isAVDev() const
{
	return mValid;
}

bool DS_DeviceDescriptor::isVideoCaptureDev() const
{
	if (mCapabilities & CAP_VIDEO_CAPTURE)
		return true;

	return false;
}

bool DS_DeviceDescriptor::isTuner() const
{
	if (mCapabilities & CAP_TUNER)
		return true;

	return false;
}

bool DS_DeviceDescriptor::isAudioDev() const
{
	if (mCapabilities & CAP_AUDIO_CAPTURE)
		return true;

	return false;
}

bool DS_DeviceDescriptor::isOverlayDev() const
{

	if (mCapabilities & CAP_VIDEO_OVERLAY)
		return true;

	return false;
}

bool DS_DeviceDescriptor::isRadioDev() const
{
	if (mCapabilities & CAP_RADIO)
		return true;

	return false;
}

bool DS_DeviceDescriptor::isVBIDev() const
{
	if (mCapabilities & CAP_VBI_CAPTURE)
		return true;

	return false;
}

bool DS_DeviceDescriptor::isVfWDevice() const
{
	// The capture filter wraps a VFW device
	QzCComPtr<IAMVfwCaptureDialogs> VfWDialog;
	if (SUCCEEDED(((IBaseFilter*)mHandle)->QueryInterface(
			IID_IAMVfwCaptureDialogs, (void**)&VfWDialog))) {
		return true;
	}

	return false;
}

void DS_DeviceDescriptor::findDevice(std::string &UniqueDeviceID,
		IBaseFilter **CaptureFilter, std::string &DeviceName,
		std::string &DeviceInfo, bool *IsVideoDevice, bool *IsAudioDevice)
{
	// Enumerate the video category
	// You can add other Category-CLSIDs to the list (e.g. audio category (gets audio capture filters))
	std::list<CLSID> EnumCategoriesList;
	EnumCategoriesList.push_back(CLSID_VideoInputDeviceCategory);
	//EnumCategoriesList.push_back(CLSID_AudioInputDeviceCategory);

	for (std::list<CLSID>::iterator EnumCategoriesListIter=
			EnumCategoriesList.begin(); EnumCategoriesListIter
			!=EnumCategoriesList.end(); EnumCategoriesListIter++) {

		// Create the system device enumerator.
		HRESULT hr;
		ICreateDevEnum *SysDevEnum = NULL;
		if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
				CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void **)&SysDevEnum))) {
			return;
		}

		// Obtain a class enumerator for the device capture category.
		IEnumMoniker *EnumCat = NULL;
		hr = SysDevEnum->CreateClassEnumerator(*EnumCategoriesListIter,
				&EnumCat, 0);

		if (hr == S_OK) {
			// Enumerate the monikers.
			IMoniker *Moniker = NULL;
			ULONG cFetched;
			while (EnumCat->Next(1, &Moniker, &cFetched) == S_OK) {
				IPropertyBag *PropBag;
				hr = Moniker->BindToStorage(0, 0, IID_IPropertyBag,
						(void **)&PropBag);

				if (SUCCEEDED(hr)) {
					VARIANT CLSIDString, FriendlyName, DevUniqueID;
					VariantInit(&CLSIDString);
					VariantInit(&FriendlyName);
					VariantInit(&DevUniqueID);
					hr = PropBag->Read(L"CLSID", &CLSIDString, NULL);
					HRESULT hr1 = PropBag->Read(L"FriendlyName", &FriendlyName, NULL);
					PropBag->Read(L"DevicePath", &DevUniqueID, NULL);

					if (SUCCEEDED(hr) && SUCCEEDED(hr1) &&
					((bstr2string(FriendlyName.bstrVal)+
									bstr2string(CLSIDString.bstrVal))==UniqueDeviceID ||
							bstr2string(DevUniqueID.bstrVal)==UniqueDeviceID)) {
						hr = Moniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&*CaptureFilter);

						if (SUCCEEDED(hr)) {
							DeviceName=bstr2string(FriendlyName.bstrVal);

							if(*EnumCategoriesListIter == CLSID_VideoInputDeviceCategory && IsVideoDevice!=NULL)
							*IsVideoDevice = true;
							else if(IsVideoDevice!=NULL)
							*IsVideoDevice=false;

							if(*EnumCategoriesListIter == CLSID_AudioInputDeviceCategory && IsAudioDevice!=NULL)
							*IsAudioDevice=true;
							else if(IsAudioDevice!=NULL)
							*IsAudioDevice=false;
						}

						// Get DV and D-VHS/MPEG camcorder devices information
						VARIANT DevInfo; VariantInit(&DevInfo);
						if(PropBag->Read(L"Description", &DevInfo, NULL)==S_OK) {
							DeviceInfo=bstr2string(DevInfo.bstrVal);
						} else {
							DeviceInfo="";
						}

						VariantClear(&DevInfo);
						VariantClear(&FriendlyName);
						VariantClear(&CLSIDString);

						return;
					}

					VariantClear(&FriendlyName);
					VariantClear(&CLSIDString);

					// Remember to release Filter later.
					PropBag->Release();
				}
				Moniker->Release();
			}
			EnumCat->Release();
		}
		SysDevEnum->Release();
	}
}

bool DS_DeviceDescriptor::createCaptureFilterGraph(std::string &UniqueDeviceID,
		IBaseFilter *CaptureFilter)
{
	// Filter graph will be released on destructor -> see above
	QzCComPtr<ICaptureGraphBuilder2> CaptureGraphBuilder;
	IGraphBuilder *FilterGraph=NULL;

	// Create the CaptureGraphBuilder.
	if (FAILED(CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL,
			CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2,
			(void**)&CaptureGraphBuilder))) {
		return false;
	}

	// Create the FilterGraphManager.
	if (FAILED(CoCreateInstance(CLSID_FilterGraph, 0, CLSCTX_INPROC_SERVER,
			IID_IGraphBuilder, (void**)&FilterGraph)))
		return false;

	// Add the filter graph to the CaptureGraphBuilder
	if (FAILED(CaptureGraphBuilder->SetFiltergraph(FilterGraph)))
		return false;

	//Add CaptureFilter to the filter graph
	if (FAILED(FilterGraph->AddFilter(CaptureFilter,L"Capture Filter" )))
		return false;

	return RenderStream(CaptureFilter, true, true);
}

bool DS_DeviceDescriptor::findOverlaySupport(IBaseFilter *CaptureFilter,
		int *Capabilities)
{
	if (CaptureFilter==NULL) {
		return false;
	}

	// Get the CaptureBuilderGraph COM-Interface from CaptureFilter
	QzCComPtr<ICaptureGraphBuilder2> CaptureGraphBuilder;
	QzCComPtr<IGraphBuilder> FilterGraph;
	if (!GetFilterGraphFromFilter(CaptureFilter, &FilterGraph,
			&CaptureGraphBuilder))
		return false;

	// Look for a video port output pin - video port means overlay
	QzCComPtr<IPin> OutputPin;
	if (SUCCEEDED(CaptureGraphBuilder->FindPin(CaptureFilter, PINDIR_OUTPUT,
			&PIN_CATEGORY_VIDEOPORT, NULL, FALSE, 0, &OutputPin))) {
		*Capabilities |= CAP_VIDEO_OVERLAY;
	}

	OutputPin.Release();
	// Look for a output pin with MEDIASUBTYPE_Overlay (mostly found on VFW capture filters)
	std::list<IPin*> PinList;
	EnumPinsOnFilter(CaptureFilter, PinList);

	for (std::list<IPin*>::iterator PinIter=PinList.begin(); PinIter
			!=PinList.end(); PinIter++) {
		std::list<AM_MEDIA_TYPE*> MediaTypeList;
		EnumMediaTypesOnPin((*PinIter), MediaTypeList);

		for (std::list<AM_MEDIA_TYPE*>::iterator Iter=MediaTypeList.begin(); Iter
				!=MediaTypeList.end(); Iter++) {
			if ((*Iter)->subtype==MEDIASUBTYPE_Overlay) {
				*Capabilities |= CAP_VIDEO_OVERLAY;
				DeleteList(MediaTypeList);
				break;
			}
		}
		DeleteList(MediaTypeList);
	}
	DeleteList(PinList);

	return true;
}

bool DS_DeviceDescriptor::findVBISupport(IBaseFilter *CaptureFilter,
		int *Capabilities)
{
	if (CaptureFilter==NULL)
		return false;

	// Get the CaptureBuilderGraph COM-Interface from CaptureFilter
	QzCComPtr<ICaptureGraphBuilder2> CaptureGraphBuilder;
	QzCComPtr<IGraphBuilder> FilterGraph;
	if (!GetFilterGraphFromFilter(CaptureFilter, &FilterGraph,
			&CaptureGraphBuilder))
		return false;

	// Look for a VBI output pin
	QzCComPtr<IPin> OutputPin;
	if (SUCCEEDED(CaptureGraphBuilder->FindPin(CaptureFilter, PINDIR_OUTPUT,
			&PIN_CATEGORY_VBI, NULL, FALSE, 0, &OutputPin))) {
		*Capabilities |= CAP_VBI_CAPTURE;
	}

	return true;
}

bool DS_DeviceDescriptor::isAudioOrVideoDevice(IBaseFilter *CaptureFilter,
		int *Capabilities)
{
	if (CaptureFilter==NULL)
		return false;

	// Get the CaptureBuilderGraph COM-Interface from CaptureFilter
	QzCComPtr<ICaptureGraphBuilder2> CaptureGraphBuilder;
	QzCComPtr<IGraphBuilder> FilterGraph;
	if (!GetFilterGraphFromFilter(CaptureFilter, &FilterGraph,
			&CaptureGraphBuilder))
		return false;

	// First look if a crossbar is present in the capture filtergraph
	// Crossbars mostly used for multi connector capture devices (e.g. tv tuner/radio tuner or multichannel framegrabber)
	CCrossbar Crossbar(CaptureGraphBuilder);
	if (Crossbar.FindAllCrossbarsAndConnectors(CaptureFilter)>0) {
		std::list<STConnector*> InputConnectors;
		std::list<STConnector*>::iterator InputConnectorIter;

		// There is an crossbar available
		InputConnectors=Crossbar.GetInputConnectorList();
		for (InputConnectorIter=InputConnectors.begin(); InputConnectorIter
				!=InputConnectors.end(); InputConnectorIter++) {
			STConnector *InputConnector=*InputConnectorIter;

			if (InputConnector->IsAudioConnector) {
				*Capabilities |= CAP_AUDIO_CAPTURE;
			}

			if (InputConnector->IsVideoConnector) {
				*Capabilities |= CAP_VIDEO_CAPTURE;
			}
		}
	} else {
		/* There is no crossbar present - enumerate all output pins on the capture filter and look for the media format of
		 each found output pin. */
		std::list<IPin*> PinList;
		EnumPinsOnFilter(CaptureFilter, PinList);

		for (std::list<IPin*>::iterator PinIter=PinList.begin(); PinIter
				!=PinList.end(); PinIter++) {
			PIN_DIRECTION PinDir;
			(*PinIter)->QueryDirection(&PinDir);

			if (PinDir==PINDIR_OUTPUT) {
				std::list<AM_MEDIA_TYPE*> MediaTypeList;
				EnumMediaTypesOnPin((*PinIter), MediaTypeList);

				for (std::list<AM_MEDIA_TYPE*>::iterator Iter=
						MediaTypeList.begin(); Iter!=MediaTypeList.end(); Iter++) {

					if ((*Iter)->majortype==MEDIATYPE_AnalogVideo || (*Iter)->majortype==MEDIATYPE_Interleaved || (*Iter)->majortype==MEDIATYPE_Video || (*Iter)->majortype==MEDIATYPE_Stream) {
						*Capabilities |= CAP_VIDEO_CAPTURE;
					}

					if ((*Iter)->majortype==MEDIATYPE_AnalogAudio || (*Iter)->majortype==MEDIATYPE_Interleaved || (*Iter)->majortype==MEDIATYPE_Audio || (*Iter)->majortype==MEDIATYPE_Stream) {
						*Capabilities |= CAP_AUDIO_CAPTURE;
					}
				}
				DeleteList(MediaTypeList);
			}
		}
		DeleteList(PinList);
	}

	return true;
}

HRESULT DS_DeviceDescriptor::addToRot(IUnknown *pUnkGraph, DWORD *pdwRegister)
{
	IMoniker * Moniker;
	IRunningObjectTable *pROT;
	HRESULT hr;

	if (!pUnkGraph || !pdwRegister)
		return E_POINTER;

	if (FAILED(GetRunningObjectTable(0, &pROT)))
		return E_FAIL;

	WCHAR wsz[128];
	char ws[128];
	wsprintf(ws, "FilterGraph %08x pid %08x\0", (DWORD_PTR)pUnkGraph,
			GetCurrentProcessId());
	MultiByteToWideChar(CP_UTF8, 0, ws, (int)strlen(ws)+1, wsz, (int)strlen(ws));

	hr = CreateItemMoniker(L"!", wsz, &Moniker);
	if (SUCCEEDED(hr)) {
		/* Use the ROTFLAGS_REGISTRATIONKEEPSALIVE to ensure a strong reference
		 to the object.  Using this flag will cause the object to remain
		 registered until it is explicitly revoked with the Revoke() method.

		 Not using this flag means that if GraphEdit remotely connects
		 to this graph and then GraphEdit exits, this object registration
		 will be deleted, causing future attempts by GraphEdit to fail until
		 this application is restarted or until the graph is registered again.*/
		hr = pROT->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pUnkGraph, Moniker, pdwRegister);

		IBindCtx *Context;
		CreateBindCtx(0,&Context);
		LPOLESTR temp;
		Moniker->GetDisplayName(Context,0,&temp);

		Context->Release();
		Moniker->Release();
	}

	pROT->Release();
	return hr;
}

void DS_DeviceDescriptor::removeFromRot(DWORD pdwRegister)
{
	IRunningObjectTable *pROT;
	if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) {
		pROT->Revoke(pdwRegister);
		pROT->Release();
	}
}

