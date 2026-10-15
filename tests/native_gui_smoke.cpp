// Windows host-level regression for the independent native Win32 GUI.
// This is not an offscreen VSTGUI screenshot and does not depend on .uidesc.
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include <windows.h>
#include <objbase.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace Steinberg;
using namespace Steinberg::Vst;
void require(bool x,const char* why){if(!x)throw std::runtime_error(why);}
class HostFrame final:public U::Implements<U::Directly<IPlugFrame>> {
public:
 HWND parent=nullptr;
 tresult PLUGIN_API resizeView(IPlugView* view,ViewRect* size) override {
  MoveWindow(parent,0,0,size->getWidth(),size->getHeight(),FALSE);
  return view->onSize(size);
 }
};
void pump(){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
int main(int argc,char** argv) {
 try {
  require(argc==2,"Pass VST3 module path");
  SetProcessDPIAware();CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  std::string error;
  auto module=VST3::Hosting::Module::create(argv[1],error);
  require(module!=nullptr,"Failed to load VST3 module");
  HostApplication host;
  module->getFactory().setHostContext(&host);
  IPtr<IEditController> controller;
  for(const auto& info:module->getFactory().classInfos())
   if(info.category()==kVstComponentControllerClass)
    controller=module->getFactory().createInstance<IEditController>(info.ID());
  require(controller!=nullptr && controller->initialize(&host)==kResultOk,"Controller init failed");
  auto view=owned(controller->createView(ViewType::kEditor));
  require(view!=nullptr,"Controller created no GUI");
  ViewRect rect{};
  require(view->getSize(&rect)==kResultTrue && rect.getWidth()>900 && rect.getHeight()>500,
          "Editor size is invalid");
  HostFrame frame;
  frame.parent=CreateWindowExW(0,L"STATIC",L"Native Win32 VST3 GUI smoke",
      WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,rect.getWidth(),rect.getHeight(),
      nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  require(frame.parent!=nullptr,"Cannot create native host window");
  require(view->setFrame(&frame)==kResultTrue,"Host setFrame failed");
  require(view->attached(frame.parent,kPlatformTypeHWND)==kResultTrue,"VST3 native attached failed");
  pump();
  HWND child=FindWindowExW(frame.parent,nullptr,L"JerzyTapeDriveNativeV1",nullptr);
  require(child!=nullptr && IsWindowVisible(child),"Plugin-owned native GUI child HWND missing");
  RECT cr{};GetClientRect(child,&cr);
  require(cr.right==rect.getWidth()&&cr.bottom==rect.getHeight(),
          "Plugin-owned native GUI child size mismatch");
  InvalidateRect(child,nullptr,TRUE);UpdateWindow(child);pump();
  // GitHub Windows runners have a noninteractive desktop. Verify the
  // plugin's real HWND WM_PRINTCLIENT GDI path, not desktop screenshots.
  HDC dc=GetDC(child);
  HDC capture=CreateCompatibleDC(dc);
  HBITMAP bitmap=CreateCompatibleBitmap(dc,rect.getWidth(),rect.getHeight());
  HGDIOBJ previous=SelectObject(capture,bitmap);
  PatBlt(capture,0,0,rect.getWidth(),rect.getHeight(),WHITENESS);
  SendMessageW(child,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(capture),PRF_CLIENT);
  const COLORREF background=GetPixel(capture,3,3),banner=GetPixel(capture,3,102),
                 card=GetPixel(capture,30,145);
  std::cerr<<"GDI capture: background="<<std::hex<<background
           <<" banner="<<banner<<" card="<<card<<std::dec
           <<" expected="<<RGB(13,21,26)<<" visibility="<<IsWindowVisible(child)
           <<" size="<<cr.right<<"x"<<cr.bottom<<"\\n";
  SelectObject(capture,previous);DeleteObject(bitmap);DeleteDC(capture);
  ReleaseDC(child,dc);
  require(background==RGB(26,41,45),"Native GUI header did not paint");
  require(banner==RGB(205,153,75),"Native GUI amber accent did not paint");
  require(card==RGB(28,36,43),"Native GUI parameter cards did not paint");
  ParameterInfo info{};
  require(controller->getParameterInfo(0,info)==kResultTrue,"First parameter not found");
  const double before=controller->getParamNormalized(info.id);
  SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(250,220));
  SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(250,220));
  require(std::abs(controller->getParamNormalized(info.id)-before)>.01,
          "Native GUI drag/click not connected to original VST3 parameter");
  SendMessageW(child,WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(250,220));
  require(std::abs(controller->getParamNormalized(info.id)-info.defaultNormalizedValue)<.0002,
          "Native GUI right-click parameter reset failed");
  ViewRect desired(0,0,930,570);
  require(frame.resizeView(view,&desired)==kResultTrue,"Host resize rejected");
  GetClientRect(child,&cr);
  require(cr.right==930&&cr.bottom==570,"GUI ignored host resize");
  require(view->removed()==kResultTrue,"Editor remove failed");
  view->setFrame(nullptr);
  view=nullptr;DestroyWindow(frame.parent);
  controller->terminate();controller=nullptr;module.reset();
  CoUninitialize();
  std::cout<<"Native Windows VST3 GUI: HWND, actual pixels, edit, reset, resizing PASS\n";
  return 0;
 } catch(const std::exception& ex) {std::cerr<<ex.what()<<'\n';return 1;}
}
