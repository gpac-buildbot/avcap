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
#include <HelpFunc.h> 
#include <ks.h>
#include <ksmedia.h>

#include "DS_Tuner.h"
#include "DS_DeviceDescriptor.h"

using namespace avcap;

#define INSTANCEDATA_OF_PROPERTY_PTR(x) ((PKSPROPERTY((x))) + 1)
#define INSTANCEDATA_OF_PROPERTY_SIZE(x) (sizeof((x)) - sizeof(KSPROPERTY))

// Construction & Destruction

DS_Tuner::DS_Tuner(DS_DeviceDescriptor *dd, int index, const std::string &name,
		int type, int caps, unsigned int high, unsigned int low) :
	mDSDeviceDescriptor(dd), mIndex(index), mName(name), mType(type),
			mCapabilities(caps), mRangeHigh(high), mRangeLow(low)
{
#ifndef __MINGW32__
	if (dd->getHandle()==NULL) {
		return;
	}

	// There can be only one Tuner in a filter graph --> we don't need the index value
	// Look for the IAMTuner COM-Interface in the filter graph
	QzCComPtr<IAMTVTuner> Tuner;
	if (!getIAMTVTunerInterfaceFromFilter(
			((IBaseFilter*)mDSDeviceDescriptor->getHandle()), &Tuner)) {
		return;
	}

	if (isRadioTuner()) {
		if (FAILED(Tuner->put_Mode(AMTUNER_MODE_FM_RADIO))) {
			return;
		}
	}

	if (isTVTuner()) {
		if (FAILED(Tuner->put_Mode(AMTUNER_MODE_TV))) {
			return;
		}
	}

	// Get some capabilites of the Tuner - it depends on the selected Tuner (radio or tv)
	HRESULT hr;
	DWORD dwSupported=0;

	// Query the IKsPropertySet on the tv Tuner filter.
	QzCComPtr<IKsPropertySet> m_pKSProp;
	if (FAILED(Tuner->QueryInterface(IID_IKsPropertySet, (void**)&m_pKSProp))) {
		return;
	}

	KSPROPERTY_TUNER_MODE_CAPS_S ModeCaps;
	memset(&ModeCaps, 0, sizeof(KSPROPERTY_TUNER_MODE_CAPS_S));
	
	if (isRadioTuner()) {
		ModeCaps.Mode = AMTUNER_MODE_FM_RADIO;
	}

	if (isTVTuner()) {
		ModeCaps.Mode = AMTUNER_MODE_TV;
	}

	// Check either the property is supported or not by the Tuner drivers 
	hr = m_pKSProp->QuerySupported(PROPSETID_TUNER, KSPROPERTY_TUNER_MODE_CAPS,
			&dwSupported);
	
	if (SUCCEEDED(hr) && dwSupported & KSPROPERTY_SUPPORT_GET) {
		DWORD cbBytes=0;
		hr = m_pKSProp->Get(PROPSETID_TUNER, KSPROPERTY_TUNER_MODE_CAPS, 
		INSTANCEDATA_OF_PROPERTY_PTR(&ModeCaps), INSTANCEDATA_OF_PROPERTY_SIZE(ModeCaps), &ModeCaps, sizeof(ModeCaps), &cbBytes);
	
		if (FAILED(hr)) {
			return;
		}
	} else {
		return;
	}

	mStep=(double)ModeCaps.TuningGranularity/1000000;
	mRangeLow=(unsigned int)(((double)ModeCaps.MinFrequency/1000000)/mStep);
	mRangeHigh=(unsigned int)(((double)ModeCaps.MaxFrequency/1000000)/mStep);
#endif
}

DS_Tuner::~DS_Tuner()
{
}

int DS_Tuner::setMono()
{
	return setAudioMode(AMTVAUDIO_MODE_MONO);
}

int DS_Tuner::setStereo()
{
	return setAudioMode(AMTVAUDIO_MODE_STEREO);
}

int DS_Tuner::setLang1()
{
	return setAudioMode(AMTVAUDIO_MODE_LANG_A);
}

int DS_Tuner::setLang2()
{
	return setAudioMode(AMTVAUDIO_MODE_LANG_B);
}

int DS_Tuner::setSAP()
{
	return setAudioMode(AMTVAUDIO_MODE_LANG_C);
}

int DS_Tuner::setAudioMode(int mode)
{
	// other modes than mono and stereo don't apply to radio Tuners 
	if (isRadioTuner()) {
		if (mode != AMTVAUDIO_MODE_MONO && mode != AMTVAUDIO_MODE_STEREO) {
			return -1;
		}
	}

	//Look for the IAMTuner COM-Interface in the FilterGraph
	QzCComPtr<IAMTVAudio> Audio;
	if (!getIAMTVAudioInterfaceFromFilter(
			((IBaseFilter*)mDSDeviceDescriptor->getHandle()), &Audio)) {
		return -1;
	}

	long TVAudioModes=0;
	HRESULT hr=Audio->GetAvailableTVAudioModes(&TVAudioModes);
	if (hr!=S_OK) {
		return -1;
	}

	// test, if requested mode is supported by the driver
	if (mode == AMTVAUDIO_MODE_MONO && (!(TVAudioModes & AMTVAUDIO_MODE_MONO))) {
		return -1;
	}

	if (mode == AMTVAUDIO_MODE_STEREO && (!(TVAudioModes
			& AMTVAUDIO_MODE_STEREO))) {
		return -1;
	}

	if (mode == AMTVAUDIO_MODE_LANG_A && (!(TVAudioModes
			& AMTVAUDIO_MODE_LANG_A))) {
		return -1;
	}

	if (mode == AMTVAUDIO_MODE_LANG_B && (!(TVAudioModes
			& AMTVAUDIO_MODE_LANG_B))) {
		return -1;
	}

	if (mode == AMTVAUDIO_MODE_LANG_C && (!(TVAudioModes
			& AMTVAUDIO_MODE_LANG_C))) {
		return -1;
	}

	if (FAILED(Audio->put_TVAudioMode(mode))) {
		return -1;
	}

	return 0;
}

int DS_Tuner::setFreq(double f)
{
#ifndef __MINGW32__
	// Set the frequency (in MHz).
	// Please also make sure to set the 
	// appropriate video standard and contry code before calling this method.

	HRESULT hr;
	DWORD dwSupported=0;

	// Look for the IAMTuner COM-Interface in the filter graph
	QzCComPtr<IAMTVTuner> Tuner;
	if (!getIAMTVTunerInterfaceFromFilter(
			((IBaseFilter*)mDSDeviceDescriptor->getHandle()), &Tuner)) {
		return false;
	}

	// Query the IKsPropertySet on the tv Tuner filter.
	QzCComPtr<IKsPropertySet> m_pKSProp;
	if (FAILED(Tuner->QueryInterface(IID_IKsPropertySet, (void**)&m_pKSProp))) {
		return -1;
	}

	KSPROPERTY_TUNER_MODE_CAPS_S ModeCaps;
	KSPROPERTY_TUNER_FREQUENCY_S Frequency;
	memset(&ModeCaps, 0, sizeof(KSPROPERTY_TUNER_MODE_CAPS_S));
	memset(&Frequency, 0, sizeof(KSPROPERTY_TUNER_FREQUENCY_S));

	if (isRadioTuner()) {
		ModeCaps.Mode = AMTUNER_MODE_FM_RADIO;
	}
	
	if (isTVTuner()) {
		ModeCaps.Mode = AMTUNER_MODE_TV;
	}

	// Check either the property is supported or not by the  drivers 
	hr = m_pKSProp->QuerySupported(PROPSETID_TUNER, KSPROPERTY_TUNER_MODE_CAPS,
			&dwSupported);
	if (SUCCEEDED(hr) && dwSupported & KSPROPERTY_SUPPORT_GET) {
		DWORD cbBytes=0;
		hr = m_pKSProp->Get(PROPSETID_TUNER, KSPROPERTY_TUNER_MODE_CAPS, 
		INSTANCEDATA_OF_PROPERTY_PTR(&ModeCaps), 
		INSTANCEDATA_OF_PROPERTY_SIZE(ModeCaps), &ModeCaps, sizeof(ModeCaps), &cbBytes);

		if (FAILED(hr)) {
			return -1;
		}
	} else {
		return -1;
	}

	Frequency.Frequency=(ULONG)(f*pow(10.0f, 6));
	Frequency.TuningFlags=KS_TUNER_TUNING_EXACT;

	// Set the frequency
	if (f<ModeCaps.MinFrequency) {
		f=ModeCaps.MinFrequency;
	}

	if (f>ModeCaps.MaxFrequency) {
		f=ModeCaps.MaxFrequency;
	}

	hr = m_pKSProp->Set(PROPSETID_TUNER, KSPROPERTY_TUNER_FREQUENCY, 
	INSTANCEDATA_OF_PROPERTY_PTR(&Frequency), 
	INSTANCEDATA_OF_PROPERTY_SIZE(Frequency), &Frequency, sizeof(Frequency));

	if (FAILED(hr)) {
		return -1;
	}
#endif

	return 0;
}

// Return th current frequency (in MHz).

double DS_Tuner::getFreq() const
{
#ifndef __MINGW32__
	// Look for the IAMTuner COM-Interface in the filter graph
	QzCComPtr<IAMTVTuner> Tuner;
	if (!getIAMTVTunerInterfaceFromFilter(
			((IBaseFilter*)mDSDeviceDescriptor->getHandle()), &Tuner)) {
		return false;
	}

	// Query the IKsPropertySet on the tv Tuner filter.
	QzCComPtr<IKsPropertySet> m_pKSProp;
	if (FAILED(Tuner->QueryInterface(IID_IKsPropertySet, (void**)&m_pKSProp))) {
		return -1;
	}

	HRESULT hr;
	DWORD dwSupported=0;
	KSPROPERTY_TUNER_STATUS_S TunerStatus;
	memset(&TunerStatus, 0, sizeof(KSPROPERTY_TUNER_STATUS_S));

	// Check either the property is supported or not by the Tuner drivers 
	hr = m_pKSProp->QuerySupported(PROPSETID_TUNER, KSPROPERTY_TUNER_STATUS,
			&dwSupported);
	
	if (SUCCEEDED(hr) && dwSupported & KSPROPERTY_SUPPORT_GET) {
		for (int i=0; i<20; i++) {
			DWORD cbBytes=0;
			hr = m_pKSProp->Get(PROPSETID_TUNER, KSPROPERTY_TUNER_STATUS, 
			INSTANCEDATA_OF_PROPERTY_PTR(&TunerStatus), 
			INSTANCEDATA_OF_PROPERTY_SIZE(TunerStatus), &TunerStatus, sizeof(TunerStatus), &cbBytes);

			if (FAILED(hr)) {
				return -1;
			}
	
			if (TunerStatus.Busy!=1) {
				return (TunerStatus.CurrentFrequency/(pow(10.0f, 6)));
			}
			
			Sleep(100);
		}
	}
#endif

	return -1;
}

int DS_Tuner::increaseFreq()
{
	// Increase the frequency one unit step.
	double freq;

	if ((freq = getFreq()) < 0.0)
		return -1;

	return setFreq(freq+mStep);

	return 0;
}

int DS_Tuner::decreaseFreq()
{
	// Decrease the frequency one unit step.
	double freq;

	if ((freq = getFreq()) < 0.0)
		return -1;

	return setFreq(freq-mStep);

	return 0;
}

int DS_Tuner::getSignalStrength() const
{
#ifndef __MINGW32__
	// Return the strength of the current signal, if available.

	// We use two ways to determine the signal strength. Why? Some hardware have problems
	// with the first way and some have problems with the second way. It's really strange!!

	// First way
	//Look for the IAMTuner COM-Interface in the FilterGraph
	QzCComPtr<IAMTVTuner> tuner;
	if (!getIAMTVTunerInterfaceFromFilter(
			((IBaseFilter*)mDSDeviceDescriptor->getHandle()), &tuner)) {
		return -1;
	}

	long SignalStrength;
	if (FAILED(tuner->SignalPresent(&SignalStrength))) {
		return -1;
	}

	if (SignalStrength==1) {
		return 0xffff;
	} // We have a signal.


	//Second way
	// Query the IKsPropertySet on the tv Tuner filter.
	QzCComPtr<IKsPropertySet> m_pKSProp;
	if (FAILED(tuner->QueryInterface(IID_IKsPropertySet, (void**)&m_pKSProp))) {
		return -1;
	}

	HRESULT hr;
	DWORD dwSupported=0;
	KSPROPERTY_TUNER_STATUS_S TunerStatus;
	memset(&TunerStatus, 0, sizeof(KSPROPERTY_TUNER_STATUS_S));

	// Check either the property is supported or not by the Tuner drivers 
	hr = m_pKSProp->QuerySupported(PROPSETID_TUNER, KSPROPERTY_TUNER_STATUS,
			&dwSupported);
	if (SUCCEEDED(hr) && dwSupported & KSPROPERTY_SUPPORT_GET) {
		// Search multiple times, because the Tuner can be busy
		for (int i=0; i<10; i++) {
			DWORD cbBytes=0;
			hr = m_pKSProp->Get(PROPSETID_TUNER, KSPROPERTY_TUNER_STATUS, 
			INSTANCEDATA_OF_PROPERTY_PTR(&TunerStatus), 
			INSTANCEDATA_OF_PROPERTY_SIZE(TunerStatus), &TunerStatus, sizeof(TunerStatus), &cbBytes);

			if (FAILED(hr)) {
				return -1;
			}

			if (TunerStatus.Busy!=1) {
				break;
			}
			Sleep(100);
		}
	} else {
		return -1;
	}

	bool DoFirstWay=false;
	bool DoSecondWay=false;

	if (TunerStatus.SignalStrength<=(ULONG)SignalStrength) {
		DoFirstWay=true;
	}
	
	if (TunerStatus.SignalStrength>(ULONG)SignalStrength) {
		DoSecondWay=true;
	}

	if (DoFirstWay && SignalStrength==1) {
		return 0xffff;
	}
	
	if (DoSecondWay && TunerStatus.SignalStrength==2) {
		return 0xffff;
	}
	
	if (DoFirstWay) {
		return SignalStrength;
	}
	
	if (DoSecondWay) {
		return TunerStatus.SignalStrength;
	}
#endif

	return -1;
}


bool DS_Tuner::getIAMTVTunerInterfaceFromFilter(IBaseFilter *Filter,
		IAMTVTuner **Tuner) const
{
	// Look upstream (starts from Filter) for a filter implementing the IAMTVTuner COM-Interface
	// Look for the IAMTuner COM-Interface in the filter graph
	// Get the filter graph from the CaptureFilter
	QzCComPtr<IGraphBuilder> FilterGraph;
	QzCComPtr<ICaptureGraphBuilder2> CaptureGraphBuilder;
	if (!GetFilterGraphFromFilter(Filter, &FilterGraph, &CaptureGraphBuilder)) {
		return false;
	}

	// Get the IAMTVTuner COM-Interface
	if (FAILED(CaptureGraphBuilder->FindInterface(&LOOK_UPSTREAM_ONLY, NULL,
			Filter, IID_IAMTVTuner, (void**)&*Tuner))) {
		return false;
	}

	return true;
}

bool DS_Tuner::getIAMTVAudioInterfaceFromFilter(IBaseFilter *Filter,
		IAMTVAudio **Audio)
{
	// Look upstream (starts from Filter) for a filter implementing the IAMTVAudio COM-Interface
	// Look for the IAMTuner COM-Interface in the filter graph
	// Get the filter graph from the capture filter
	QzCComPtr<IGraphBuilder> FilterGraph;
	QzCComPtr<ICaptureGraphBuilder2> CaptureGraphBuilder;
	if (!GetFilterGraphFromFilter(Filter, &FilterGraph, &CaptureGraphBuilder)) {
		return false;
	}

	// Get the IAMTVTuner COM-Interface
	if (FAILED(CaptureGraphBuilder->FindInterface(&LOOK_UPSTREAM_ONLY, NULL,
			Filter, IID_IAMTVAudio, (void**)&*Audio))) {
		return false;
	}

	return true;
}
