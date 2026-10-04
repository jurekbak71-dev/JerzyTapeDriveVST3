#pragma once
#include "vstgui/plugin-bindings/vst3editor.h"

namespace JerzyAudio {

class TapeDriveEditor : public VSTGUI::VST3Editor {
public:
    using VSTGUI::VST3Editor::VST3Editor;
    ~TapeDriveEditor() override;
    void valueChanged(VSTGUI::CControl* control) override;

protected:
    void PLUGIN_API close() override;
    bool PLUGIN_API open(void* parent,const VSTGUI::PlatformType& type) override;
#ifdef VST3_CONTENT_SCALE_SUPPORT
    Steinberg::tresult PLUGIN_API setContentScaleFactor(ScaleFactor factor) override;
#endif
    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* rect) override;
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* rect) override;
    bool beforeSizeChange(const VSTGUI::CRect& newSize,const VSTGUI::CRect& oldSize) override;

private:
    struct NativeResizeWatcher;
    NativeResizeWatcher* nativeWatcher=nullptr;
    bool applyingSize=false;
    double userZoom=1.0;
    double hostScaleFactor=1.0;
    void fitHostSize(const Steinberg::ViewRect& size);
    void applyZoom(double factor);
};

}
