#include "tapedrive_controller.h"
#include "tapedrive_params.h"
#include "tapedrive_gui_views.h"
#include "tapedrive_editor.h"
#include "tapedrive_state.h"
#include <cmath>
#include "base/source/fstreamer.h"
#include "public.sdk/source/vst/vstparameters.h"
#include <cstring>

using namespace Steinberg; using namespace Steinberg::Vst;
namespace JerzyAudio {
namespace {
class LogFrequencyParameter : public RangeParameter {
public:
 LogFrequencyParameter(const TChar* name,ParamID id,double low,double high,double def)
 : RangeParameter(name,id,STR16("Hz"),low,high,def),lo(low),hi(high){
  setNormalized(toNormalized(def)); setPrecision(0);
 }
 ParamValue toPlain(ParamValue value) const override { return lo*std::pow(hi/lo,value); }
 ParamValue toNormalized(ParamValue value) const override { return std::log(std::max(lo,value)/lo)/std::log(hi/lo); }
private: double lo,hi;
};
}


tresult PLUGIN_API TapeDriveController::initialize(FUnknown*c){
 registerTapeDriveViews();
 auto r=EditControllerEx1::initialize(c); if(r!=kResultOk)return r;
 auto add=[this](const TChar*n,ParamID id,const TChar*u,double lo,double hi,double def,int prec,int32 steps=0,int32 flags=ParameterInfo::kCanAutomate){
  auto*p=new RangeParameter(n,id,u,lo,hi,def,steps,flags); p->setPrecision(prec); parameters.addParameter(p);
 };
 add(STR16("Tape Sat"),kSatId,STR16("%"),0,100,35,1);
 add(STR16("Level"),kLevelId,STR16("dB"),-24,12,-6,1);
 add(STR16("Dry"),kDryId,STR16("%"),0,100,0,1);
 add(STR16("Gain"),kGainModeId,STR16(""),0,1,0,0,1);
 add(STR16("Shift"),kShiftId,STR16(""),0,2,1,0,2);
 add(STR16("Bypass"),kDriveBypassId,STR16(""),0,1,0,0,1,ParameterInfo::kCanAutomate|ParameterInfo::kIsBypass);
 parameters.addParameter(new LogFrequencyParameter(STR16("HPF"),kHPFCutoffId,20,2000,20));
 add(STR16("HPF Res"),kHPFResId,STR16("Q"),0.5,12.0,0.707,2);
 parameters.addParameter(new LogFrequencyParameter(STR16("LPF"),kLPFCutoffId,1000,20000,20000));
 add(STR16("LPF Res"),kLPFResId,STR16("Q"),0.5,12.0,0.707,2);
 add(STR16("Wow"),kWowFlutterId,STR16("%"),0,100,0,1);
 auto* mode=new StringListParameter(STR16("Preamp Mode"),kPreampModeId);
 mode->appendString(STR16("OFF")); mode->appendString(STR16("TUBE")); mode->appendString(STR16("TRANSISTOR"));
 parameters.addParameter(mode);
 add(STR16("Preamp Drive"),kPreampDriveId,STR16("%"),0,100,20,1);
 add(STR16("Flutter"),kFlutterId,STR16("%"),0,100,0,1);
 add(STR16("Tape Age"),kTapeAgeId,STR16("%"),0,100,0,1);
 add(STR16("Opto Compression"),kOptoAmountId,STR16("%"),0,100,35,1);
 add(STR16("Opto Makeup"),kOptoMakeupId,STR16("dB"),-12,12,0,1);
 add(STR16("Opto Bypass"),kOptoBypassId,STR16(""),0,1,0,0,1);
 add(STR16("Opto Gain Reduction"),kOptoMeterId,STR16("dB"),0,24,0,1,0,ParameterInfo::kIsReadOnly);
 add(STR16("Input Meter"),kInputMeterId,STR16(""),0,1,0,2,0,ParameterInfo::kIsReadOnly);
 add(STR16("Saturation Meter"),kSaturationMeterId,STR16(""),0,1,0,2,0,ParameterInfo::kIsReadOnly);
 add(STR16("Output Meter"),kDriveMeterId,STR16(""),0,1,0,2,0,ParameterInfo::kIsReadOnly);
 return kResultOk;
}

tresult PLUGIN_API TapeDriveController::setComponentState(IBStream*s){
 if(!s)return kResultFalse;
 IBStreamer b(s,kLittleEndian);
 TapeDriveParams p;
 if(!readTapeDriveState(b,p)) return kResultFalse;
 const ParamID ids[]={kSatId,kLevelId,kDryId,kGainModeId,kShiftId,kDriveBypassId,
   kHPFCutoffId,kHPFResId,kLPFCutoffId,kLPFResId,kWowId,kPreampModeId,kPreampDriveId,
   kFlutterId,kTapeAgeId,kOptoAmountId,kOptoMakeupId,kOptoBypassId};
 const double values[]={p.sat,p.level,p.dry,p.gainMode,p.shift,p.bypass,
   p.hpfCutoff,p.hpfRes,p.lpfCutoff,p.lpfRes,p.wowFlutter,p.preampMode,p.preampDrive,
   p.flutter,p.tapeAge,p.optoAmount,p.optoMakeup,p.optoBypass};
 for(int i=0;i<18;++i) setParamNormalized(ids[i],values[i]);
 return kResultOk;
}

IPlugView* PLUGIN_API TapeDriveController::createView(const char*n){
 if(n&&std::strcmp(n,ViewType::kEditor)==0){
   auto* editor = new TapeDriveEditor(this,"view","tapedrive_vintage.uidesc");
   editor->setAllowedZoomFactors({0.70,0.85,1.0,1.25,1.5});
   return editor;
 }
 return nullptr;
}

}
