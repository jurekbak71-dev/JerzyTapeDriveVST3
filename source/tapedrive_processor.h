#pragma once
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "tapedrive_dsp.h"
#include "tapedrive_params.h"

namespace JerzyAudio {
class TapeDriveProcessor : public Steinberg::Vst::AudioEffect {
public:
 TapeDriveProcessor();
 static Steinberg::FUnknown* createInstance(void*) { return static_cast<Steinberg::Vst::IAudioProcessor*>(new TapeDriveProcessor); }
 Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) SMTG_OVERRIDE;
 Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement*,Steinberg::int32,Steinberg::Vst::SpeakerArrangement*,Steinberg::int32) SMTG_OVERRIDE;
 Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup&) SMTG_OVERRIDE;
 Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool) SMTG_OVERRIDE;
 Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32) SMTG_OVERRIDE;
 Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData&) SMTG_OVERRIDE;
 Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream*) SMTG_OVERRIDE;
 Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream*) SMTG_OVERRIDE;
private:
 void readChanges(Steinberg::Vst::IParameterChanges*);
 void sendMeters(Steinberg::Vst::ProcessData&,double,double,double,double);
 TapeDriveParams p{};
 TapeDriveDSP<float> dsp32;
 TapeDriveDSP<double> dsp64;
 double sr=44100.0;
 double inMeter=0.0,satMeter=0.0,outMeter=0.0,grMeter=0.0;
};
}
