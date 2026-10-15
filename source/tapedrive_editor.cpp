#include "tapedrive_editor.h"
#include "tapedrive_editor_geometry.h"
#include "vstgui/lib/controls/ccontrol.h"
#include <cmath>

namespace JerzyAudio {
#ifdef VST3_CONTENT_SCALE_SUPPORT
Steinberg::tresult PLUGIN_API TapeDriveEditor::setContentScaleFactor(ScaleFactor factor) {
    if (!std::isfinite(factor) || factor <= 0)
        return Steinberg::kInvalidArgument;
    const auto result = VSTGUI::VST3Editor::setContentScaleFactor(factor);
    // Before attachment there is no native frame yet. Keep the VST3 host's
    // initial editor rectangle in sync with VSTGUI's own DPI factor.
    if (result == Steinberg::kResultOk && !getFrame()) {
        setRect(Steinberg::ViewRect(0, 0,
            TapeDriveEditorGeometry::pixelWidth(getZoomFactor(),factor),
            TapeDriveEditorGeometry::pixelHeight(getZoomFactor(),factor)));
    }
    return result;
}
#endif

void TapeDriveEditor::valueChanged(VSTGUI::CControl* control) {
    if (control && control->getTag() >= 9001 && control->getTag() <= 9004) {
        if (control->getValueNormalized() > 0.5f) {
            static const double zooms[] = {0.70, 0.85, 1.00, 1.25};
            // VSTGUI handles the native HWND, resize negotiation and DPI.
            setZoomFactor(zooms[control->getTag()-9001]);
        }
        return;
    }
    VSTGUI::VST3Editor::valueChanged(control);
}
}
