// Windows integration test: opens the actual built VST3 editor in a native HWND.
// Parameter lookup and real mouse clicks verify the transformed control areas,
// rather than merely testing size calculations or a mock rendering backend.
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstplugview.h"
#include "../source/tapedrive_params.h"
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
    HWND window=nullptr;
    bool reject=false;
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
    const int px=static_cast<int>(std::lround(x*r.getWidth()/1200.0));
    const int py=static_cast<int>(std::lround(y*r.getHeight()/672.0));
    HWND child=editorWindow(parent);
    SendMessageW(child,WM_MOUSEMOVE,0,MAKELPARAM(px,py));
    SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(px,py));
    SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(px,py));pump();
}
void checkPanel(IPlugView* view,IEditController* controller,HWND parent){
    const auto r=sizeOf(view);RECT child{};GetClientRect(editorWindow(parent),&child);
    require(std::abs(child.right-r.getWidth())<=1&&std::abs(child.bottom-r.getHeight())<=1,"Native child size differs from accepted host viewport");
    FUnknownPtr<IParameterFinder> finder(view);
    require(finder!=nullptr,"Editor has no parameter finder");
    struct Point{double x,y;ParamID id;};
    for(const auto& p:{Point{141,319,kSatId},Point{591,475,kTapeAgeId},Point{985,614,kDriveBypassId},Point{941,483,kOptoMakeupId}}){
        ParamID id=0;
        require(finder->findParameter(static_cast<int32>(std::lround(p.x*r.getWidth()/1200.0)),
                                     static_cast<int32>(std::lround(p.y*r.getHeight()/672.0)),id)==kResultTrue&&id==p.id,
                "GUI occupies only part of the window: a scaled control cannot be found");
    }
    if(r.getWidth()==1980){
        require(renderProbe(view,"tapedrive-render-1980x1108.png")==1,"Rendered panel leaves blank pixels at the window edges");
    }
    const double before=controller->getParamNormalized(kDriveBypassId);
    click(view,parent,985,614);
    require(controller->getParamNormalized(kDriveBypassId)!=(before),"Mouse click at scaled bypass position did not change the parameter");
    click(view,parent,985,614);
    require(controller->getParamNormalized(kDriveBypassId)==before,"Bypass click did not toggle back");
}
int main(int argc,char** argv){try{
    require(argc==2,"Pass the built Tape Drive VST3 path");
    SetProcessDPIAware();CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    std::string error;auto module=VST3::Hosting::Module::create(argv[1],error);
    if(!module)throw std::runtime_error(error);
    const auto nativeModule=GetModuleHandleA(argv[1]);
    renderProbe=reinterpret_cast<RenderProbe>(GetProcAddress(nativeModule,"JerzyRenderEditorForTest"));
    require(renderProbe!=nullptr,"Render probe missing from test build");
    visibleProbe=reinterpret_cast<RenderProbe>(GetProcAddress(nativeModule,"JerzyCaptureVisibleEditorForTest"));
    resetProbe=reinterpret_cast<ResetProbe>(GetProcAddress(nativeModule,"JerzyResetTransformForTest"));
    require(visibleProbe && resetProbe,"Visible window probes missing");
    HostApplication host;module->getFactory().setHostContext(&host);
    IPtr<IEditController> controller;
    for(const auto& info:module->getFactory().classInfos())
        if(info.category()==kVstComponentControllerClass)controller=module->getFactory().createInstance<IEditController>(info.ID());
    require(controller!=nullptr,"Tape Drive controller not found");
    require(controller->initialize(&host)==kResultOk,"Controller initialization failed");
    auto view=owned(controller->createView(ViewType::kEditor));require(view!=nullptr,"createView failed");
    HostFrame frame;
    frame.window=CreateWindowExW(0,L"STATIC",L"Tape Drive native GUI regression",WS_POPUP,0,0,1200,672,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    require(frame.window!=nullptr,"Cannot create test host HWND");
    FUnknownPtr<IPlugViewContentScaleSupport> scale(view);
    require(scale!=nullptr,"Editor has no DPI support");
    // Hosts can send DPI and restored sizes before a native frame exists.
    require(scale->setContentScaleFactor(1.25f)==kResultTrue,"Pre-attach DPI failed");
    auto initial=sizeOf(view);require(initial.getWidth()==1500&&initial.getHeight()==840,"Pre-attach DPI must affect advertised editor size");
    MoveWindow(frame.window,0,0,initial.getWidth(),initial.getHeight(),FALSE);
    view->setFrame(&frame);
    require(view->attached(frame.window,kPlatformTypeHWND)==kResultTrue,"Cannot attach real VST3 GUI");pump();
    checkPanel(view,controller,frame.window);
    for(float dpi:{1.0f,1.25f,1.5f,2.0f}){
        require(scale->setContentScaleFactor(dpi)==kResultTrue,"Runtime DPI change failed");
        for(const auto& b:{std::pair<double,double>{1067,602},{1127,602},{1067,632},{1127,632}}){
            const double zoom=b.second==602?(b.first==1067?.75:1.0):(b.first==1067?1.25:1.5);
            click(view,frame.window,b.first,b.second);
            const auto r=sizeOf(view);
            require(r.getWidth()==static_cast<int>(std::lround(1200*zoom*dpi))&&r.getHeight()==static_cast<int>(std::lround(672*zoom*dpi)),"Zoom button did not resize the actual viewport correctly");
            checkPanel(view,controller,frame.window);
        }
        // Reproduce a host which bypasses checkSizeConstraint or restores an
        // oversized / slightly different-aspect window (as in the FL screenshot).
        ViewRect restored(0,0,1980,1108);
        require(frame.resizeView(view,&restored)==kResultTrue,"Direct host resize was rejected");
        checkPanel(view,controller,frame.window);
        // Reproduce Windows wrappers which resize native windows without the
        // VST3 onSize callback. The original integration test missed this path.
        MoveWindow(frame.window,0,0,1200,672,FALSE);pump();
        MoveWindow(frame.window,0,0,1980,1108,FALSE);pump();
        auto nativeSize=sizeOf(view);
        require(nativeSize.getWidth()==1980&&nativeSize.getHeight()==1108,"Parent HWND resize without onSize left the panel at its old size");
        checkPanel(view,controller,frame.window);
        MoveWindow(editorWindow(frame.window),0,0,1500,840,FALSE);pump();
        nativeSize=sizeOf(view);
        require(nativeSize.getWidth()==1500&&nativeSize.getHeight()==840,"Child HWND resize without onSize left the panel at its old size");
        checkPanel(view,controller,frame.window);
        frame.reject=true;const auto old=sizeOf(view);
        click(view,frame.window,1127,602);
        auto unchanged=sizeOf(view);require(old.getWidth()==unchanged.getWidth()&&old.getHeight()==unchanged.getHeight(),"Rejected resize changed the accepted viewport");
        checkPanel(view,controller,frame.window);frame.reject=false;
        std::cout<<"Real HWND GUI: DPI "<<dpi<<", all zoom buttons, parameter hit areas, bypass mouse clicks, VST3 and native HWND resize, and rejection OK\n";
    }
    // Actual WM_PAINT and desktop pixels, with the whole client area visible.
    for(const auto dimensions:{std::pair<int,int>{900,504},{1000,560}}){
        ViewRect visible(0,0,dimensions.first,dimensions.second);
        frame.resizeView(view,&visible);
        SetWindowPos(frame.window,HWND_TOPMOST,0,0,dimensions.first,dimensions.second,SWP_SHOWWINDOW);
        ShowWindow(frame.window,SW_SHOW);settle();
        const std::string path="tapedrive-window-"+std::to_string(dimensions.first)+".bmp";
        require(visibleProbe(view,path.c_str())==1,"Visible HWND pixels differ from the scaled panel");
        // Size alone is insufficient: simulate a late VSTGUI transform reset.
        require(resetProbe(view)==1,"Cannot simulate a stale transform");settle();
        checkPanel(view,controller,frame.window);
        require(visibleProbe(view,path.c_str())==1,"Late transform reset left the visible panel unscaled");
    }
    view->removed();view->setFrame(nullptr);scale=nullptr;view=nullptr;
    DestroyWindow(frame.window);controller->terminate();controller=nullptr;module.reset();CoUninitialize();
    std::cout<<"Native Tape Drive GUI integration OK\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
