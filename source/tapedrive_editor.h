#pragma once
#include "vstgui/plugin-bindings/vst3editor.h"

namespace JerzyAudio {
// Use the official VST3Editor window lifecycle and DPI/zoom implementation.
// Never subclass DAW-owned HWNDs, install plugin timers, or override CFrame transforms.
class TapeDriveEditor : public VSTGUI::VST3Editor {
public:
    using VSTGUI::VST3Editor::VST3Editor;
    void valueChanged(VSTGUI::CControl* control) override;
#ifdef VST3_CONTENT_SCALE_SUPPORT
    Steinberg::tresult PLUGIN_API setContentScaleFactor(ScaleFactor factor) override;
#endif
};
}
