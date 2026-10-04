// Windows integration test: opens the actual built VST3 editor in a native HWND.
// Parameter lookup and real mouse clicks verify vector scaling and hit areas.
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstplugview.h"
#include "../source/tapedrive_params.h"
#include "../source/tapedrive_editor_geometry.h"
#include <windows.h>
#include <objbase.h>
#include <utility>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace JerzyAudio;
using RenderProbe=int(__cdecl*)(IPlugView*,const char*);
RenderProbe renderProbe=nullptr;
RenderProbe visibleProbe=nullptr;
using ResetProbe=int(__cdecl*)(IPlugView*);
ResetProbe resetProbe=nullptr;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
class HostFrame final : public U::Implements<U::Directly<IPlugFrame>> {
public:
    HWND window=nullptr;bool reject=false;
    tresult PLUGIN_API resizeView(IPlugView* view,ViewRect* size) override {
        if(reject)return kResultFalse;
        MoveWindow(window,0,0,size->getWidth(),size->getHeight(),FALSE);
        return view->onSize(size);
    }
};
void pump(){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
void settle(){const auto end=GetTickCount64()+400;do{pump();MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);}while(GetTickCount64()<end);}
HWND editorWindow(HWND parent){
    HWND largest=nullptr;long area=0;
    for(HWND child=GetWindow(parent,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
        RECT r{};GetClientRect(child,&r);const long a=(r.right-r.left)*(r.bottom-r.top);
        if(a>area){largest=child;area=a;}
    }
    require(largest!=nullptr,"Native editor child window not created");return largest;
}
ViewRect sizeOf(IPlugView* view){ViewRect r;require(view->getSize(&r)==kResultTrue,"getSize failed");return r;}
void click(IPlugView* view,HWND parent,double x,double y){
    const auto r=sizeOf(view);
    const int px=static_cast<int>(std::lround(x*r.getWidth()/TapeDriveEditorGeometry::width));
    const int py=static_cast<int>(std::lround(y*r.getHeight()/TapeDriveEditorGeometry::height));
    HWND child=editorWindow(parent);
    SendMessageW(child,WM_MOUSEMOVE,0,MAKELPARAM(px,py));
    SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(px,py));
    SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(px,py));pump();
}
void checkPanel(IPlugView* view,IEditController* controller,HWND parent){
    const auto r=sizeOf(view);RECT child{};GetClientRect(editorWindow(parent),&child);
    require(std::abs(child.right-r.getWidth())<=1&&std::abs(child.bottom-r.getHeight())<=1,
            "Native child size differs from accepted host viewport");
    FUnknownPtr<IParameterFinder> finder(view);require(finder!=nullptr,"Editor has no parameter finder");
    struct Point{double x,y;ParamID id;};
    for(const auto& p:{Point{119,181,kSatId},Point{468,330,kTapeAgeId},Point{877,507,kDriveBypassId},Point{801,300,kOptoMakeupId}}){
        ParamID id=0;
        require(finder->findParameter(static_cast<int32>(std::lround(p.x*r.getWidth()/TapeDriveEditorGeometry::width)),
                                     static_cast<int32>(std::lround(p.y*r.getHeight()/TapeDriveEditorGeometry::height)),id)==kResultTrue&&id==p.id,
                "A vector-scaled control cannot be found at its transformed position");
    }
    if(r.getWidth()==1664){
        require(renderProbe(view,"tapedrive-render-1664x1000.png")==1,
                "Vector panel leaves blank pixels at a forced host viewport");
    }
    const double before=controller->getParamNormalized(kDriveBypassId);
    click(view,parent,877,507);
    require(controller->getParamNormalized(kDriveBypassId)!=(before),"Scaled master bypass click did not change the parameter");
    click(view,parent,877,507);
    require(controller->getParamNormalized(kDriveBypassId)==before,"Master bypass click did not toggle back");
}
int main(int argc,char** argv){try{
    require(argc==2,"Pass the built Tape Drive VST3 path");
    SetProcessDPIAware();CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    std::string error;auto module=VST3::Hosting::Module::create(argv[1],error);
    if(!module)throw std::runtime_error(error);
    const auto nativeModule=GetModuleHandleA(argv[1]);
    renderProbe=reinterpret_cast<RenderProbe>(GetProcAddress(nativeModule,"JerzyRenderEditorForTest"));
    visibleProbe=reinterpret_cast<RenderProbe>(GetProcAddress(nativeModule,"JerzyCaptureVisibleEditorForTest"));
    resetProbe=reinterpret_cast<ResetProbe>(GetProcAddress(nativeModule,"JerzyResetTransformForTest"));
    require(renderProbe&&visibleProbe&&resetProbe,"TapeDrive GUI test probes missing");

    HostApplication host;module->getFactory().setHostContext(&host);
    IPtr<IEditController> controller;
    for(const auto& info:module->getFactory().classInfos())
        if(info.category()==kVstComponentControllerClass)controller=module->getFactory().createInstance<IEditController>(info.ID());
    require(controller!=nullptr,"Tape Drive controller not found");
    require(controller->initialize(&host)==kResultOk,"Controller initialization failed");
    auto view=owned(controller->createView(ViewType::kEditor));require(view!=nullptr,"createView failed");

    HostFrame frame;
    frame.window=CreateWindowExW(0,L"STATIC",L"Tape Drive vector GUI regression",WS_POPUP,0,0,1040,640,
                                 nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    require(frame.window!=nullptr,"Cannot create test host HWND");
    FUnknownPtr<IPlugViewContentScaleSupport> scale(view);require(scale!=nullptr,"Editor has no DPI support");

    require(scale->setContentScaleFactor(1.25f)==kResultTrue,"Pre-attach DPI failed");
    auto initial=sizeOf(view);
    require(initial.getWidth()==1300&&initial.getHeight()==800,
            "Pre-attach DPI must resize once, not compound through VSTGUI zoom plus custom transform");
    MoveWindow(frame.window,0,0,initial.getWidth(),initial.getHeight(),FALSE);
    view->setFrame(&frame);
    require(view->attached(frame.window,kPlatformTypeHWND)==kResultTrue,"Cannot attach real VST3 GUI");pump();
    checkPanel(view,controller,frame.window);

    struct ZoomButton{double x,y,zoom;};
    const ZoomButton buttons[]={{939,490.5,.70},{986,490.5,.85},{939,519.5,1.00},{986,519.5,1.25}};
    for(float dpi:{1.0f,1.25f,1.5f,2.0f}){
        require(scale->setContentScaleFactor(dpi)==kResultTrue,"Runtime DPI change failed");
        for(const auto& b:buttons){
            click(view,frame.window,b.x,b.y);
            const auto r=sizeOf(view);
            require(r.getWidth()==static_cast<int>(std::lround(1040*b.zoom*dpi))&&
                    r.getHeight()==static_cast<int>(std::lround(640*b.zoom*dpi)),
                    "UI scale button produced a double-scaled or incorrect viewport");
            checkPanel(view,controller,frame.window);
        }

        // Reproduce a host restoring an arbitrary, non-template rectangle.
        ViewRect restored(0,0,1664,1000);
        require(frame.resizeView(view,&restored)==kResultTrue,"Direct host resize was rejected");
        checkPanel(view,controller,frame.window);

        // Reproduce a Windows wrapper that changes HWND dimensions without a VST3 callback.
        MoveWindow(frame.window,0,0,1040,640,FALSE);pump();
        MoveWindow(frame.window,0,0,1664,1000,FALSE);pump();
        auto nativeSize=sizeOf(view);
        require(nativeSize.getWidth()==1664&&nativeSize.getHeight()==1000,
                "Parent HWND resize without onSize left the vector panel at its old size");
        checkPanel(view,controller,frame.window);

        MoveWindow(editorWindow(frame.window),0,0,1300,800,FALSE);pump();
        nativeSize=sizeOf(view);
        require(nativeSize.getWidth()==1300&&nativeSize.getHeight()==800,
                "Child HWND resize without onSize left the vector panel at its old size");
        checkPanel(view,controller,frame.window);

        frame.reject=true;const auto old=sizeOf(view);
        click(view,frame.window,986,519.5);
        auto unchanged=sizeOf(view);
        require(old.getWidth()==unchanged.getWidth()&&old.getHeight()==unchanged.getHeight(),
                "Rejected resize changed the accepted viewport");
        checkPanel(view,controller,frame.window);frame.reject=false;
        std::cout<<"Real HWND vector GUI: DPI "<<dpi<<", zoom buttons, hit areas, direct/native resize and rejection OK\n";
    }

    // Actual WM_PAINT/desktop pixels at small practical window sizes.
    for(const auto dimensions:{std::pair<int,int>{832,512},{936,576}}){
        ViewRect visible(0,0,dimensions.first,dimensions.second);
        frame.resizeView(view,&visible);
        SetWindowPos(frame.window,HWND_TOPMOST,0,0,dimensions.first,dimensions.second,SWP_SHOWWINDOW);
        ShowWindow(frame.window,SW_SHOW);settle();
        const std::string path="tapedrive-window-"+std::to_string(dimensions.first)+".bmp";
        require(visibleProbe(view,path.c_str())==1,"Visible HWND pixels differ from the vector reference");
        require(resetProbe(view)==1,"Cannot simulate a stale transform");settle();
        checkPanel(view,controller,frame.window);
        require(visibleProbe(view,path.c_str())==1,"Late transform reset left the visible panel unscaled");
    }

    view->removed();view->setFrame(nullptr);scale=nullptr;view=nullptr;
    DestroyWindow(frame.window);controller->terminate();controller=nullptr;module.reset();CoUninitialize();
    std::cout<<"Native Tape Drive vector GUI integration OK\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
