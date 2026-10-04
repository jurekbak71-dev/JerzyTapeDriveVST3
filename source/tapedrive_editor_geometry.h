#pragma once
#include <algorithm>
#include <cmath>
namespace JerzyAudio {
struct TapeDriveEditorGeometry {
    static constexpr double width=1200.0,height=672.0;
    static constexpr double minZoom=0.75,maxZoom=1.5;
    static double dpi(double scale){return std::isfinite(scale)&&scale>0.0?scale:1.0;}
    static double zoomForWidth(double pixels,double scale){
        return std::clamp(pixels/(width*dpi(scale)),minZoom,maxZoom);
    }
    static int pixelWidth(double zoom,double scale){return static_cast<int>(std::lround(width*zoom*dpi(scale)));}
    static int pixelHeight(double zoom,double scale){return static_cast<int>(std::lround(height*zoom*dpi(scale)));}
};
}
