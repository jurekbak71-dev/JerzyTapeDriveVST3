#include "tapedrive_controller.h"
#include "tapedrive_ids.h"
#include "tapedrive_processor.h"
#include "version.h"
#include "public.sdk/source/main/pluginfactory_constexpr.h"
using namespace Steinberg; using namespace Steinberg::Vst;
BEGIN_FACTORY_DEF(stringCompanyName,stringCompanyWeb,stringCompanyEmail,2)
DEF_CLASS(JerzyAudio::kTapeDriveProcessorUID,PClassInfo::kManyInstances,kVstAudioEffectClass,"Jerzy Tape Drive",Vst::kDistributable,"Fx|Distortion",FULL_VERSION_STR,kVstVersionString,JerzyAudio::TapeDriveProcessor::createInstance,nullptr)
DEF_CLASS(JerzyAudio::kTapeDriveControllerUID,PClassInfo::kManyInstances,kVstComponentControllerClass,"Jerzy Tape Drive Controller",0,"",FULL_VERSION_STR,kVstVersionString,JerzyAudio::TapeDriveController::createInstance,nullptr)
END_FACTORY
