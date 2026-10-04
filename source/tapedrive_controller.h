#pragma once
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"
namespace JerzyAudio {
class TapeDriveController:public Steinberg::Vst::EditControllerEx1{
public:
 static Steinberg::FUnknown* createInstance(void*){return static_cast<Steinberg::Vst::IEditController*>(new TapeDriveController);}
 Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) SMTG_OVERRIDE;
 Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream*) SMTG_OVERRIDE;
 Steinberg::IPlugView* PLUGIN_API createView(const char*) SMTG_OVERRIDE;
};
}
