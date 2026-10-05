#include "tapedrive_processor.h"
#include "tapedrive_ids.h"
#include "tapedrive_state.h"
#include "tapedrive_automation.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>

using namespace Steinberg; using namespace Steinberg::Vst;
namespace JerzyAudio {

TapeDriveProcessor::TapeDriveProcessor(){ setControllerClass(kTapeDriveControllerUID); }

tresult PLUGIN_API TapeDriveProcessor::initialize(FUnknown* c){
 auto r=AudioEffect::initialize(c); if(r!=kResultOk)return r;
 addAudioInput(STR16("Stereo In"),SpeakerArr::kStereo);
 addAudioOutput(STR16("Stereo Out"),SpeakerArr::kStereo);
 return kResultOk;
}

tresult PLUGIN_API TapeDriveProcessor::setBusArrangements(SpeakerArrangement* i,int32 ni,SpeakerArrangement* o,int32 no){
 if(ni!=1||no!=1)return kResultFalse;
 auto a=SpeakerArr::getChannelCount(i[0]); auto b=SpeakerArr::getChannelCount(o[0]);
 if(a!=b||(a!=1&&a!=2))return kResultFalse;
 return AudioEffect::setBusArrangements(i,ni,o,no);
}

tresult PLUGIN_API TapeDriveProcessor::setupProcessing(ProcessSetup& s){
 sr=s.sampleRate>0?s.sampleRate:44100;
 dsp32.prepare(sr,2); dsp64.prepare(sr,2);
 inMeter=satMeter=outMeter=grMeter=0.0;
 return AudioEffect::setupProcessing(s);
}

tresult PLUGIN_API TapeDriveProcessor::setActive(TBool s){
 if(s){
  dsp32.reset(); dsp64.reset();
  inMeter=satMeter=outMeter=grMeter=0.0;
 }
 return AudioEffect::setActive(s);
}

tresult PLUGIN_API TapeDriveProcessor::canProcessSampleSize(int32 s){
 return(s==kSample32||s==kSample64)?kResultTrue:kResultFalse;
}

void TapeDriveProcessor::sendMeters(ProcessData& d,double inPk,double satPk,double outPk,double grPk){
 const double decay=std::exp(-std::max(0,d.numSamples)/(0.30*sr));
 const auto smooth=[decay](double value,double oldValue){
  value=std::clamp(value,0.0,1.0);
  return std::max(value,oldValue*decay);
 };
 inMeter=smooth(std::pow(std::clamp(inPk,0.0,1.0),0.35),inMeter);
 satMeter=smooth(std::clamp(satPk,0.0,1.0),satMeter);
 outMeter=smooth(std::pow(std::clamp(outPk,0.0,1.0),0.35),outMeter);

 grMeter=smooth(grPk,grMeter);
 if(!d.outputParameterChanges)return;
 struct M { ParamID id; double value; } meters[]={
  {kInputMeterId,inMeter},{kSaturationMeterId,satMeter},{kDriveMeterId,outMeter},{kOptoMeterId,grMeter}
 };
 for(const auto& m:meters){
  int32 queueIndex=0;
  if(auto*q=d.outputParameterChanges->addParameterData(m.id,queueIndex)){
   int32 pointIndex=0;
   q->addPoint(std::max<int32>(0,d.numSamples-1),m.value,pointIndex);
  }
 }
}

tresult PLUGIN_API TapeDriveProcessor::process(ProcessData& d){
 double inPk=0.0,satPk=0.0,outPk=0.0,grPk=0.0;
 const int ch=(d.numInputs>0 && d.numOutputs>0)
     ?std::clamp(std::min(d.inputs[0].numChannels,d.outputs[0].numChannels),0,2):0;
 // Parameter-only flushes still consume every point without touching audio.
 processTapeDriveAutomation(d.inputParameterChanges,p,std::max(0,d.numSamples),
     [&](int32 start,int32 count){
      if(ch==0)return;
      double segIn=0.0,segSat=0.0,segOut=0.0,segGr=0.0;
      const auto render=[&](auto** input,auto** output,auto& dsp){
       using Sample=std::remove_pointer_t<std::remove_pointer_t<decltype(input)>>;
       const Sample* in[2]{};Sample* out[2]{};
       for(int c=0;c<ch;++c){
        in[c]=input && input[c]?input[c]+start:nullptr;
        out[c]=output && output[c]?output[c]+start:nullptr;
       }
       if(p.bypass>=0.5){
        for(int c=0;c<ch;++c){
         if(in[c] && out[c] && in[c]!=out[c])std::memcpy(out[c],in[c],sizeof(Sample)*count);
         for(int i=0;i<count;++i){
          if(in[c])segIn=std::max(segIn,std::abs(static_cast<double>(in[c][i])));
          if(out[c])segOut=std::max(segOut,std::abs(static_cast<double>(out[c][i])));
         }
        }
       }else dsp.process(in,out,ch,count,p,segIn,segSat,segOut,segGr);
      };
      if(d.symbolicSampleSize==kSample32)
       render(d.inputs[0].channelBuffers32,d.outputs[0].channelBuffers32,dsp32);
      else if(d.symbolicSampleSize==kSample64)
       render(d.inputs[0].channelBuffers64,d.outputs[0].channelBuffers64,dsp64);
      inPk=std::max(inPk,segIn);satPk=std::max(satPk,segSat);
      outPk=std::max(outPk,segOut);grPk=std::max(grPk,segGr);
     });
 if(d.numOutputs>0 && d.numSamples>0)d.outputs[0].silenceFlags=0;
 sendMeters(d,inPk,satPk,outPk,grPk);
 return kResultOk;
}

tresult PLUGIN_API TapeDriveProcessor::setState(IBStream*s){
 if(!s)return kResultFalse;
 IBStreamer b(s,kLittleEndian);
 if(!readTapeDriveState(b,p)) return kResultFalse;
 return kResultOk;
}

tresult PLUGIN_API TapeDriveProcessor::getState(IBStream*s){
 if(!s)return kResultFalse;
 IBStreamer b(s,kLittleEndian);
 if(!writeTapeDriveState(b,p)) return kResultFalse;
 return kResultOk;
}

}
