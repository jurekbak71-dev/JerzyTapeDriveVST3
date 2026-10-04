// Only compiled with BUILD_TESTING. Render the actual DLL's CFrame through its
// graphics backend so integration tests can inspect pixels, not just hit areas.
#include "../source/tapedrive_editor.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/platform/iplatformframe.h"
#include "vstgui/lib/coffscreencontext.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/platform/platformfactory.h"
#include <fstream>
#include <iostream>
extern "C" __declspec(dllexport) int __cdecl JerzyRenderEditorForTest(Steinberg::IPlugView* view,const char* path){
    auto* editor=dynamic_cast<JerzyAudio::TapeDriveEditor*>(view);
    if(!editor || !editor->getFrame())return 0;
    auto* frame=editor->getFrame();
    const auto rect=frame->getViewSize();
    auto context=VSTGUI::COffscreenContext::create({rect.getWidth(),rect.getHeight()});
    if(!context)return 0;
    context->beginDraw();
    context->setFillColor(VSTGUI::CColor(255,0,255));
    context->drawRect(rect,VSTGUI::kDrawFilled);
    frame->drawRect(context,rect);
    context->endDraw();
    auto* bitmap=context->getBitmap();
    const auto png=VSTGUI::getPlatformFactory().createBitmapMemoryPNGRepresentation(bitmap->getPlatformBitmap());
    std::ofstream image(path,std::ios::binary);
    image.write(reinterpret_cast<const char*>(png.data()),png.size());
    auto pixels=VSTGUI::owned(VSTGUI::CBitmapPixelAccess::create(bitmap));
    if(!pixels)return 0;
    // Procedural steel near all four edges must be opaque and painted; blank\n    // right/bottom strips or a stale vector transform must fail this test.
    for(double y:{.05,.95})for(double x:{.035,.965}){
        int painted=0;
        for(int j=-3;j<=3;++j)for(int i=-3;i<=3;++i){
            pixels->setPosition(static_cast<uint32_t>(x*rect.getWidth()+i),static_cast<uint32_t>(y*rect.getHeight()+j));
            VSTGUI::CColor c;pixels->getColor(c);
            if(c.alpha>240 && c.red+c.green+c.blue>10 && !(c.red>240&&c.blue>240&&c.green<10))++painted;
        }
        std::cout<<"Rendered edge patch "<<x<<','<<y<<": "<<painted<<"/49 painted pixels\n";
        if(painted<40)return 0;
    }
    return !png.empty();
}

// Capture pixels actually presented on the Windows desktop. The offscreen
// probe above cannot detect a stale/undersized DirectComposition surface.
#include <windows.h>
#include <vector>
#include "vstgui/lib/cgraphicstransform.h"
extern "C" __declspec(dllexport) int __cdecl JerzyResetTransformForTest(Steinberg::IPlugView* view){
    auto* editor=dynamic_cast<JerzyAudio::TapeDriveEditor*>(view);
    if(!editor || !editor->getFrame())return 0;
    editor->getFrame()->setTransform(VSTGUI::CGraphicsTransform());
    return 1;
}
extern "C" __declspec(dllexport) int __cdecl JerzyCaptureVisibleEditorForTest(Steinberg::IPlugView* view,const char* path){
    auto* editor=dynamic_cast<JerzyAudio::TapeDriveEditor*>(view);
    if(!editor || !editor->getFrame())return 0;
    auto* frame=editor->getFrame();const auto r=frame->getViewSize();
    const int width=static_cast<int>(r.getWidth()),height=static_cast<int>(r.getHeight());
    HWND window=static_cast<HWND>(frame->getPlatformFrame()->getPlatformRepresentation());
    POINT origin{};ClientToScreen(window,&origin);
    if(!IsWindowVisible(window) || origin.x<0 || origin.y<0 ||
       origin.x+width>GetSystemMetrics(SM_CXSCREEN) || origin.y+height>GetSystemMetrics(SM_CYSCREEN))return 0;
    HDC screen=GetDC(nullptr),copy=CreateCompatibleDC(screen);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* data=nullptr;HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&data,nullptr,0);
    if(!bitmap){DeleteDC(copy);ReleaseDC(nullptr,screen);return 0;}
    const auto old=SelectObject(copy,bitmap);
    const bool copied=BitBlt(copy,0,0,width,height,screen,origin.x,origin.y,SRCCOPY|CAPTUREBLT)!=0;
    GdiFlush();
    auto reference=VSTGUI::COffscreenContext::create({r.getWidth(),r.getHeight()});
    reference->beginDraw();frame->drawRect(reference,r);reference->endDraw();
    auto pixels=VSTGUI::owned(VSTGUI::CBitmapPixelAccess::create(reference->getBitmap()));
    const auto* bgra=static_cast<const unsigned char*>(data);
    bool matches=copied && pixels;
    // Vector borders, controls and meters: enough detail to detect a stale\n    // transform even if the host fills unused space with dark pixels.
    for(const auto p:{VSTGUI::CPoint{.035,.05},VSTGUI::CPoint{.965,.95},
         VSTGUI::CPoint{.14,.12},VSTGUI::CPoint{.865,.12},
         VSTGUI::CPoint{119./1040,181./640},VSTGUI::CPoint{468./1040,330./640},
         VSTGUI::CPoint{706./1040,173./640},VSTGUI::CPoint{903./1040,173./640},VSTGUI::CPoint{877./1040,507./640}}){
        int matching=0;
        for(int dy=-6;dy<=6;++dy)for(int dx=-6;dx<=6;++dx){
            const int x=static_cast<int>(p.x*width)+dx,y=static_cast<int>(p.y*height)+dy;
            VSTGUI::CColor c;pixels->setPosition(x,y);pixels->getColor(c);
            const auto* q=bgra+4*(y*width+x);
            const int error=std::abs(int(q[0])-c.blue)+std::abs(int(q[1])-c.green)+std::abs(int(q[2])-c.red);
            if(error<60)++matching;
        }
        std::cout<<"Visible Windows patch "<<p.x<<','<<p.y<<": "<<matching<<"/169 pixels match\n";
        matches=matches && matching>=135;
    }
    BITMAPFILEHEADER header{};header.bfType=0x4D42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
    header.bfSize=header.bfOffBits+width*height*4;
    std::ofstream output(path,std::ios::binary);
    output.write(reinterpret_cast<const char*>(&header),sizeof(header));
    output.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(info.bmiHeader));
    output.write(reinterpret_cast<const char*>(data),width*height*4);
    SelectObject(copy,old);DeleteObject(bitmap);DeleteDC(copy);ReleaseDC(nullptr,screen);
    return matches?1:0;
}
