#pragma once
// Jerzy Audio native Win32 VST3 editor, 2026-10.
// Deliberately self-contained: no VSTGUI, UIDescription XML, JUCE,
// DirectComposition, bitmaps, or resources are required to display the GUI.
// The DAW's HWND is NEVER subclassed or resized; only a plugin-owned child.
// Original VST3 processor/controller class IDs and all parameter IDs remain intact.
#if !defined(_WIN32)
#error This native editor is intended for Windows VST3.
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <cwchar>
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

namespace JerzyAudio {

class NativeWindowsEditor final : public Steinberg::Vst::EditorView {
public:
    explicit NativeWindowsEditor(Steinberg::Vst::EditController* c, const wchar_t* title,
                                 const wchar_t* windowClass)
        : Steinberg::Vst::EditorView(c), title_(title), className_(windowClass) {
        setRect(Steinberg::ViewRect(0,0,kWidth,kHeight));
        const auto count = c->getParameterCount();
        for (Steinberg::int32 i=0;i<count;++i) {
            Steinberg::Vst::ParameterInfo info{};
            if(c->getParameterInfo(i,info)==Steinberg::kResultTrue)
                params_.push_back(info);
        }
    }

    ~NativeWindowsEditor() override { removeWindow(); }

    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override {
        return type && !std::strcmp(type, Steinberg::kPlatformTypeHWND)
            ? Steinberg::kResultTrue : Steinberg::kResultFalse;
    }

    Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) override {
        if(isPlatformTypeSupported(type)!=Steinberg::kResultTrue || !parent || !IsWindow(static_cast<HWND>(parent)))
            return Steinberg::kResultFalse;
        // The controller's editorAttached callback still runs as usual.
        auto result=Steinberg::Vst::EditorView::attached(parent,type);
        if(result!=Steinberg::kResultTrue && result!=Steinberg::kResultOk) return result;

        // Window class belongs to THIS VST3 module, never FL Studio's EXE.
        // Registering against the host EXE can leave a dangling WndProc after a
        // plugin unload/reload during rescanning, causing the next GUI to fail.
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&NativeWindowsEditor::windowProc),
                               &module_)) {
            Steinberg::Vst::EditorView::removed(); return Steinberg::kResultFalse;
        }
        WNDCLASSEXW wc{};
        wc.cbSize=sizeof(wc);
        wc.style=CS_DBLCLKS|CS_HREDRAW|CS_VREDRAW;
        wc.lpfnWndProc=&NativeWindowsEditor::windowProc;
        wc.hInstance=module_;
        wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
        wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
        wc.lpszClassName=className_;
        if(!RegisterClassExW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) {
            Steinberg::Vst::EditorView::removed(); return Steinberg::kResultFalse;
        }

        const auto r=getRect();
        child_=CreateWindowExW(0,className_,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
            0,0,r.getWidth(),r.getHeight(),static_cast<HWND>(parent),nullptr,
            wc.hInstance,this);
        if(!child_) {
            Steinberg::Vst::EditorView::removed(); return Steinberg::kResultFalse;
        }
        ShowWindow(child_,SW_SHOW);
        UpdateWindow(child_);
        SetTimer(child_,kRefreshTimer,120,nullptr);
        return Steinberg::kResultTrue;
    }

    Steinberg::tresult PLUGIN_API removed() override {
        finishGesture();
        removeWindow();
        return Steinberg::Vst::EditorView::removed();
    }

    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* r) override {
        if(!r || r->getWidth()<100 || r->getHeight()<100)return Steinberg::kInvalidArgument;
        auto result=Steinberg::Vst::EditorView::onSize(r);
        if(child_ && IsWindow(child_)) {
            SetWindowPos(child_,nullptr,0,0,r->getWidth(),r->getHeight(),
                         SWP_NOZORDER|SWP_NOACTIVATE);
            InvalidateRect(child_,nullptr,FALSE);
        }
        return result;
    }

    Steinberg::tresult PLUGIN_API canResize() override { return Steinberg::kResultTrue; }
    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* r) override {
        if(!r)return Steinberg::kInvalidArgument;
        r->right=r->left+std::max(680,r->getWidth());
        r->bottom=r->top+std::max(420,r->getHeight());
        return Steinberg::kResultTrue;
    }

private:
    static constexpr int kWidth=1160,kHeight=720,kCols=4,kRows=4,kPerPage=kCols*kRows;
    static constexpr UINT_PTR kRefreshTimer=7892;
    HWND child_=nullptr;
    HMODULE module_=nullptr;
    const wchar_t* title_;
    const wchar_t* className_;
    std::vector<Steinberg::Vst::ParameterInfo> params_;
    std::vector<double> lastValues_;
    int page_=0,active_=-1;
    bool editing_=false;
    double gestureStart_=0;
    int startX_=0,startY_=0;

    static HBRUSH brush(COLORREF color) {return CreateSolidBrush(color);}
    static void fill(HDC dc,int x,int y,int w,int h,COLORREF color) {
        RECT r{x,y,x+w,y+h};HBRUSH b=brush(color);FillRect(dc,&r,b);DeleteObject(b);
    }
    static void line(HDC dc,int x,int y,int xx,int yy,COLORREF color,int width=1) {
        HPEN p=CreatePen(PS_SOLID,width,color);HGDIOBJ old=SelectObject(dc,p);
        MoveToEx(dc,x,y,nullptr);LineTo(dc,xx,yy);SelectObject(dc,old);DeleteObject(p);
    }
    static void circle(HDC dc,int x,int y,int r,COLORREF color) {
        HPEN p=CreatePen(PS_SOLID,1,color);HBRUSH b=brush(color);
        auto op=SelectObject(dc,p),ob=SelectObject(dc,b);
        Ellipse(dc,x-r,y-r,x+r,y+r);SelectObject(dc,ob);SelectObject(dc,op);
        DeleteObject(p);DeleteObject(b);
    }
    static void text(HDC dc,const wchar_t* str,int x,int y,int w,int h,
                     int size,COLORREF color,bool bold=false,UINT align=DT_LEFT|DT_VCENTER|DT_SINGLELINE) {
        HFONT font=CreateFontW(size,0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
            DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
        auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);
        RECT r{x,y,x+w,y+h};DrawTextW(dc,str,-1,&r,align|DT_END_ELLIPSIS|DT_NOPREFIX);
        SelectObject(dc,old);DeleteObject(font);
    }
    static std::wstring asWide(const Steinberg::Vst::String128& str) {
        return reinterpret_cast<const wchar_t*>(str);
    }
    Steinberg::Vst::EditController* controller() const {return getController(); }
    int pages() const {return std::max(1,int((params_.size()+kPerPage-1)/kPerPage));}
    void changePage(int n) {
        finishGesture();page_=std::max(0,std::min(pages()-1,n));
        if(child_)InvalidateRect(child_,nullptr,FALSE);
    }
    void removeWindow() {
        if(child_ && IsWindow(child_)) {
            KillTimer(child_,kRefreshTimer);
            HWND old=child_;child_=nullptr;
            DestroyWindow(old);
        }
        if(module_) {
            // Only the last editor instance succeeds in unregistering a shared class.
            UnregisterClassW(className_,module_);
            module_=nullptr;
        }
    }
    void finishGesture() {
        if(editing_ && active_>=0 && size_t(active_)<params_.size())
            controller()->endEdit(params_[active_].id);
        editing_=false;active_=-1;
        if(child_ && GetCapture()==child_)ReleaseCapture();
    }
    double normalized(int index) const {
        return std::clamp(controller()->getParamNormalized(params_[index].id),0.0,1.0);
    }
    void sendValue(int index,double value) {
        if(index<0 || index>=int(params_.size()))return;
        const auto& p=params_[index];
        if(p.flags & Steinberg::Vst::ParameterInfo::kIsReadOnly)return;
        value=std::clamp(value,0.0,1.0);
        if(p.stepCount>0) value=std::round(value*p.stepCount)/double(p.stepCount);
        controller()->setParamNormalized(p.id,value);
        controller()->performEdit(p.id,value);
        if(child_)InvalidateRect(child_,nullptr,FALSE);
    }
    struct Card {int x,y,w,h;};
    static Card cardAt(int slot) {
        return {18+(slot%kCols)*286,128+(slot/kCols)*129,276,120};
    }
    static int hitCard(int x,int y) {
        for(int i=0;i<kPerPage;++i) {
            const auto c=cardAt(i);
            if(x>=c.x&&x<c.x+c.w&&y>=c.y&&y<c.y+c.h)return i;
        }
        return -1;
    }
    void pointerDown(int x,int y,bool right) {
        if(y<110) {
            if(x<170)changePage(page_-1);
            else if(x>990)changePage(page_+1);
            return;
        }
        const int cell=hitCard(x,y),index=page_*kPerPage+cell;
        if(cell<0 || index>=int(params_.size()))return;
        const auto& p=params_[index];
        if(p.flags & Steinberg::Vst::ParameterInfo::kIsReadOnly)return;
        if(right) {
            controller()->beginEdit(p.id);
            sendValue(index,p.defaultNormalizedValue);
            controller()->endEdit(p.id);
            return;
        }
        finishGesture();
        active_=index;editing_=true;startX_=x;startY_=y;gestureStart_=normalized(index);
        controller()->beginEdit(p.id);
        if(child_)SetCapture(child_);
        if(p.stepCount==1) {
            sendValue(index,gestureStart_<.5?1.0:0.0);
            finishGesture();
        } else {
            const auto c=cardAt(cell);
            if(y>=c.y+72 && y<=c.y+109) {
                sendValue(index, double(x-(c.x+20))/double(c.w-40));
            }
        }
    }
    void pointerDrag(int x,int y) {
        if(!editing_||active_<0)return;
        // Vertical drag from current value, with fine adjustment with Shift.
        const double sensitivity=(GetKeyState(VK_SHIFT)<0)?0.001:0.005;
        sendValue(active_,gestureStart_+(startY_-y)*sensitivity+(x-startX_)*sensitivity*0.20);
    }
    void drawCard(HDC dc,int slot) {
        int index=page_*kPerPage+slot;
        const auto c=cardAt(slot);
        fill(dc,c.x,c.y,c.w,c.h,RGB(28,36,43));
        line(dc,c.x+5,c.y+5,c.x+c.w-5,c.y+5,RGB(72,84,89));
        if(index>=int(params_.size()))return;
        const auto& p=params_[index];
        const bool readonly=(p.flags & Steinberg::Vst::ParameterInfo::kIsReadOnly)!=0;
        const bool selected=(active_==index);
        const COLORREF amber=RGB(235,178,89), cream=RGB(235,231,215);
        const auto label=asWide(p.title);
        const double value=normalized(index);
        Steinberg::Vst::String128 display{};
        const bool valid=(controller()->getParamStringByValue(p.id,value,display)==Steinberg::kResultTrue);
        auto displayStr=valid?asWide(display):std::to_wstring(int(value*100.0));
        if(selected)line(dc,c.x+2,c.y+3,c.x+c.w-2,c.y+3,amber,3);
        text(dc,label.c_str(),c.x+18,c.y+15,c.w-35,24,18,cream,true);
        text(dc,displayStr.c_str(),c.x+18,c.y+45,c.w-35,24,18,
            readonly?RGB(110,166,179):amber,false,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        fill(dc,c.x+18,c.y+88,c.w-36,12,RGB(13,20,25));
        fill(dc,c.x+18,c.y+88,int((c.w-36)*value),12,
            readonly?RGB(90,166,184):amber);
        // A hardware indicator, independent of all image and XML resources.
        circle(dc,c.x+c.w-30,c.y+54,5,
            selected?RGB(120,230,145):readonly?RGB(90,117,130):amber);
    }
    void render(HDC dc,int width,int height) {
        if(width<=0||height<=0)return;
        SetMapMode(dc,MM_ANISOTROPIC);SetWindowExtEx(dc,kWidth,kHeight,nullptr);
        SetViewportExtEx(dc,width,height,nullptr);
        fill(dc,0,0,kWidth,kHeight,RGB(13,21,26));
        fill(dc,0,0,kWidth,105,RGB(26,41,45));
        fill(dc,0,101,kWidth,4,RGB(205,153,75));
        text(dc,title_,210,15,740,42,28,RGB(242,225,187),true,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        text(dc,L"NATIVE WINDOWS EDITOR  |  NO XML / GPU DEPENDENCIES",
            210,59,740,25,14,RGB(148,178,175),false,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        fill(dc,18,24,138,53,RGB(57,70,71));
        text(dc,L"< PREVIOUS",20,24,133,53,18,RGB(243,203,129),true,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        fill(dc,1004,24,138,53,RGB(57,70,71));
        text(dc,L"NEXT >",1007,24,130,53,18,RGB(243,203,129),true,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        for(int i=0;i<kPerPage;++i)drawCard(dc,i);
        fill(dc,0,657,kWidth,63,RGB(23,36,41));
        wchar_t meta[300]{};
        std::swprintf(meta,300,L"PAGE %d / %d    |    %d PARAMETERS    |    DRAG OR CLICK TO EDIT  /  RIGHT-CLICK TO RESET",
                      page_+1,pages(),int(params_.size()));
        text(dc,meta,27,666,1090,34,17,RGB(203,212,204),false,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    void paint() {
        if(!child_)return;
        PAINTSTRUCT ps{};HDC dc=BeginPaint(child_,&ps);
        RECT r{};GetClientRect(child_,&r);
        const int width=r.right-r.left,height=r.bottom-r.top;
        HDC mem=CreateCompatibleDC(dc);
        HBITMAP bitmap=CreateCompatibleBitmap(dc,std::max(1,width),std::max(1,height));
        auto old=SelectObject(mem,bitmap);
        render(mem,width,height);
        BitBlt(dc,0,0,width,height,mem,0,0,SRCCOPY);
        SelectObject(mem,old);DeleteObject(bitmap);DeleteDC(mem);
        EndPaint(child_,&ps);
    }
    static LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
        auto* self=reinterpret_cast<NativeWindowsEditor*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(msg==WM_NCCREATE) {
            auto* create=reinterpret_cast<CREATESTRUCTW*>(l);
            self=static_cast<NativeWindowsEditor*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
            return TRUE;
        }
        if(!self)return DefWindowProcW(hwnd,msg,w,l);
        switch(msg) {
        case WM_PAINT:self->paint();return 0;
        case WM_PRINTCLIENT: {
            RECT bounds{};GetClientRect(hwnd,&bounds);
            self->render(reinterpret_cast<HDC>(w),
                         static_cast<int>(bounds.right),static_cast<int>(bounds.bottom));
            return 0;
        }
        case WM_ERASEBKGND:return 1;
        case WM_TIMER:
            if(w==kRefreshTimer) {
                bool changed=false;
                if(self->lastValues_.size()!=self->params_.size()) {
                    self->lastValues_.resize(self->params_.size(),-1);changed=true;
                }
                for(size_t i=0;i<self->params_.size();++i) {
                    double v=self->normalized(int(i));
                    if(std::abs(v-self->lastValues_[i])>1e-6) {changed=true;self->lastValues_[i]=v;}
                }
                if(changed)InvalidateRect(hwnd,nullptr,FALSE);
            }
            return 0;
        case WM_LBUTTONDOWN: {
            RECT r{};GetClientRect(hwnd,&r);
            self->pointerDown(GET_X_LPARAM(l)*kWidth/std::max(1,static_cast<int>(r.right)),
                              GET_Y_LPARAM(l)*kHeight/std::max(1,static_cast<int>(r.bottom)),false);
            return 0;
        }
        case WM_RBUTTONDOWN: {
            RECT r{};GetClientRect(hwnd,&r);
            self->pointerDown(GET_X_LPARAM(l)*kWidth/std::max(1,static_cast<int>(r.right)),
                              GET_Y_LPARAM(l)*kHeight/std::max(1,static_cast<int>(r.bottom)),true);
            return 0;
        }
        case WM_MOUSEMOVE:
            if(self->editing_&&(w&MK_LBUTTON)) {
                RECT r{};GetClientRect(hwnd,&r);
                self->pointerDrag(GET_X_LPARAM(l)*kWidth/std::max(1,static_cast<int>(r.right)),
                                  GET_Y_LPARAM(l)*kHeight/std::max(1,static_cast<int>(r.bottom)));
            }
            return 0;
        case WM_LBUTTONUP:self->finishGesture();return 0;
        case WM_CAPTURECHANGED:self->editing_=false;self->active_=-1;return 0;
        case WM_MOUSEWHEEL:self->changePage(self->page_+(GET_WHEEL_DELTA_WPARAM(w)<0?1:-1));return 0;
        case WM_KEYDOWN:
            if(w==VK_PRIOR||w==VK_LEFT)self->changePage(self->page_-1);
            if(w==VK_NEXT||w==VK_RIGHT)self->changePage(self->page_+1);
            return 0;
        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);
            if(self->child_==hwnd)self->child_=nullptr;
            return DefWindowProcW(hwnd,msg,w,l);
        default:break;
        }
        return DefWindowProcW(hwnd,msg,w,l);
    }
};
} // namespace JerzyAudio
