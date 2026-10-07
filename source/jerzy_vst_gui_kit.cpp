#include "jerzy_vst_gui_kit.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cgradient.h"
#include "vstgui/lib/cfont.h"
#include <algorithm>
#include <cmath>

using namespace VSTGUI;
namespace JerzyAudio {
namespace {

JerzyGuiTheme activeTheme = jerzyThemePetrol();

void ellipse(CDrawContext* c,double x,double y,double radius,CColor fill,CColor frame){
    c->setFillColor(fill); c->setFrameColor(frame); c->setLineWidth(1.0);
    c->drawEllipse({x-radius,y-radius,x+radius,y+radius},kDrawFilledAndStroked);
}
void gradientRect(CDrawContext* c,const CRect& r,CColor a,CColor b){
    auto* path=c->createGraphicsPath();
    if(!path)return;
    path->addRect(r);
    auto* gradient=CGradient::create(0,1,a,b);
    if(gradient){c->fillLinearGradient(path,*gradient,{r.left,r.top},{r.left,r.bottom});gradient->forget();}
    path->forget();
}
void label(CDrawContext* c,const char* s,const CRect& r,double size,CColor color){
    c->setFont(kNormalFont,size);c->setFontColor(color);c->drawString(s,r,kCenterText);
}
void metalKnob(CDrawContext* c,const CRect& r,double value){
    const auto& t=getJerzyGuiTheme();
    const double x=r.getCenter().x,y=r.getCenter().y;
    const double radius=std::min(r.getWidth(),r.getHeight())*.36;
    c->setDrawMode(kAntiAliasing|kNonIntegralMode);
    for(int i=0;i<=20;++i){
        const double a=(135.0+13.5*i)*Constants::pi/180.0;
        const double rr=i%5==0?1.8:1.15;
        const auto minor=CColor(
            static_cast<uint8_t>(t.accent.red*.82),
            static_cast<uint8_t>(t.accent.green*.82),
            static_cast<uint8_t>(t.accent.blue*.82));
        ellipse(c,x+std::cos(a)*radius*1.34,y+std::sin(a)*radius*1.34,rr,i%5==0?t.accent:minor,CColor(80,57,27));
    }
    ellipse(c,x+2.8,y+4.2,radius+3.2,CColor(0,0,0,150),CColor(0,0,0,80));
    ellipse(c,x,y,radius+1.7,CColor(17,20,18),CColor(125,112,88));
    for(int i=0;i<28;++i){
        const double a=2.0*Constants::pi*i/28.0;
        c->setFrameColor(i%2?CColor(31,32,29):CColor(77,72,61));c->setLineWidth(1.0);
        c->drawLine({x+radius*.92*std::cos(a),y+radius*.92*std::sin(a)},
                    {x+radius*1.10*std::cos(a),y+radius*1.10*std::sin(a)});
    }
    const double cap=radius*.78;
    CDrawContext::PointList wedge;
    for(int i=0;i<144;++i){
        const double a=2.0*Constants::pi*i/144.0,next=a+2.0*Constants::pi/144.0;
        const double light=.58+.30*std::cos(a-.75)+.10*std::cos(5*a);
        const auto shade=static_cast<uint8_t>(std::clamp(70.0+145.0*light,72.0,225.0));
        c->setFillColor(CColor(shade,static_cast<uint8_t>(shade*.97),static_cast<uint8_t>(shade*.88)));
        wedge={{x,y},{x+cap*std::cos(a),y+cap*std::sin(a)},{x+cap*std::cos(next),y+cap*std::sin(next)}};
        c->drawPolygon(wedge,kDrawFilled);
    }
    ellipse(c,x,y,cap,CColor(0,0,0,0),CColor(222,210,183));
    ellipse(c,x-cap*.25,y-cap*.28,cap*.20,CColor(255,255,255,34),CColor(255,255,255,0));
    const double a=(135.0+270.0*std::clamp(value,0.0,1.0))*Constants::pi/180.0;
    c->setFrameColor(CColor(255,232,185));c->setLineWidth(std::max(2.0,radius*.075));
    c->drawLine({x+radius*.18*std::cos(a),y+radius*.18*std::sin(a)},
                {x+radius*.78*std::cos(a),y+radius*.78*std::sin(a)});
}

} // anonymous

const JerzyGuiTheme& jerzyThemePetrol(){
    static const JerzyGuiTheme t{
        CColor(18,66,64),CColor(5,27,29),CColor(16,44,43),CColor(7,24,25),
        CColor(241,166,73),CColor(239,226,196),CColor(112,132,117),CColor(68,220,96),CColor(241,166,73)};
    return t;
}
const JerzyGuiTheme& jerzyThemeBrass(){
    static const JerzyGuiTheme t{
        CColor(140,100,37),CColor(24,19,11),CColor(73,54,21),CColor(29,22,12),
        CColor(242,188,92),CColor(241,224,190),CColor(151,118,64),CColor(68,220,96),CColor(242,188,92)};
    return t;
}
const JerzyGuiTheme& jerzyThemeSteel(){
    static const JerzyGuiTheme t{
        CColor(57,74,88),CColor(13,20,25),CColor(32,44,53),CColor(14,23,29),
        CColor(115,200,232),CColor(216,229,233),CColor(117,142,156),CColor(83,220,118),CColor(115,200,232)};
    return t;
}
const JerzyGuiTheme& jerzyThemeBurgundy(){
    static const JerzyGuiTheme t{
        CColor(110,37,33),CColor(22,12,10),CColor(53,24,21),CColor(28,13,12),
        CColor(230,148,67),CColor(236,215,185),CColor(139,80,61),CColor(85,220,110),CColor(230,148,67)};
    return t;
}
const JerzyGuiTheme& jerzyThemeViolet(){
    static const JerzyGuiTheme t{
        CColor(65,56,109),CColor(16,14,30),CColor(36,32,63),CColor(22,19,40),
        CColor(169,148,237),CColor(229,224,241),CColor(105,93,148),CColor(89,222,133),CColor(104,198,216)};
    return t;
}
void setJerzyGuiTheme(const JerzyGuiTheme& t){activeTheme=t;}
const JerzyGuiTheme& getJerzyGuiTheme(){return activeTheme;}

JerzyKnob::JerzyKnob(const CRect& r,IControlListener* l,int32_t tag):CKnob(r,l,tag,nullptr,nullptr){
    setStartAngle(static_cast<float>(Constants::pi*.75));setRangeAngle(static_cast<float>(Constants::pi*1.5));setWheelInc(.01f);
}
void JerzyKnob::draw(CDrawContext* c){metalKnob(c,getViewSize(),getValueNormalized());setDirty(false);}
CMouseEventResult JerzyKnob::onMouseDown(CPoint& where,const CButtonState& buttons){
    if(buttons.isRightButton()){beginEdit();setValue(getDefaultValue());valueChanged();invalid();endEdit();return kMouseDownEventHandledButDontNeedMovedOrUpEvents;}
    return CKnob::onMouseDown(where,buttons);
}

JerzyAnalogMeter::JerzyAnalogMeter(const CRect& r,IControlListener* l,int32_t tag):CKnob(r,l,tag,nullptr,nullptr){setMouseEnabled(false);}
void JerzyAnalogMeter::draw(CDrawContext* c){
    const auto& t=getJerzyGuiTheme();const CRect r(getViewSize());
    c->setDrawMode(kAntiAliasing|kNonIntegralMode);c->setFillColor(CColor(8,18,18));c->setFrameColor(t.edge);c->setLineWidth(2);c->drawRect(r,kDrawFilledAndStroked);
    CRect face(r);face.inset(6,6);gradientRect(c,face,CColor(157,90,35),CColor(255,225,154));
    const double cx=face.getCenter().x,cy=face.bottom-7,rad=std::min(face.getWidth()*.48,face.getHeight()*.86);
    for(int i=0;i<=10;++i){const double a=(210+12*i)*Constants::pi/180.0;c->setFrameColor(i>=8?CColor(191,47,25):CColor(61,37,18));c->setLineWidth(i%2?1:1.6);c->drawLine({cx+rad*.77*std::cos(a),cy+rad*.77*std::sin(a)},{cx+rad*.94*std::cos(a),cy+rad*.94*std::sin(a)});}
    label(c,"-20     -10      -3      0   +3",{face.left,face.top+6,face.right,face.top+19},9,CColor(60,34,14));label(c,"VU",{face.left,face.bottom-27,face.right,face.bottom-11},13,CColor(60,34,14));
    const double a=(210+120*std::clamp(static_cast<double>(getValueNormalized()),0.0,1.0))*Constants::pi/180.0;c->setFrameColor(CColor(81,27,15));c->setLineWidth(2);c->drawLine({cx,cy},{cx+rad*.87*std::cos(a),cy+rad*.87*std::sin(a)});ellipse(c,cx,cy,3,CColor(63,38,18),CColor(63,38,18));setDirty(false);
}

JerzyToggle::JerzyToggle(const CRect& r,IControlListener* l,int32_t tag):CKnob(r,l,tag,nullptr,nullptr){setMin(0);setMax(1);setWheelInc(1);}
CMouseEventResult JerzyToggle::onMouseDown(CPoint&,const CButtonState& buttons){
    if(buttons.isRightButton()){beginEdit();setValue(getDefaultValue());valueChanged();invalid();endEdit();return kMouseDownEventHandledButDontNeedMovedOrUpEvents;}
    if(!buttons.isLeftButton())return kMouseEventNotHandled;beginEdit();setValueNormalized(getValueNormalized()<.5f?1.f:0.f);valueChanged();invalid();endEdit();return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}
void JerzyToggle::draw(CDrawContext* c){
    const auto& t=getJerzyGuiTheme();const CRect r(getViewSize());const auto p=r.getCenter();c->setDrawMode(kAntiAliasing|kNonIntegralMode);
    ellipse(c,p.x,p.y,15,CColor(91,80,63),t.edge);ellipse(c,p.x,p.y,11,CColor(8,18,18),t.text);const double end=p.y+(getValueNormalized()>=.5f?-11:11);c->setLineWidth(7);c->setFrameColor(CColor(182,167,142));c->drawLine(p,{p.x,end});ellipse(c,p.x,end,6,CColor(221,207,179),t.edge);setDirty(false);
}
JerzyThreeWay::JerzyThreeWay(const CRect& r,IControlListener* l,int32_t tag):JerzyToggle(r,l,tag){setMax(2);}
CMouseEventResult JerzyThreeWay::onMouseDown(CPoint& where,const CButtonState& buttons){
    if(buttons.isRightButton())return JerzyToggle::onMouseDown(where,buttons);if(!buttons.isLeftButton())return kMouseEventNotHandled;const int current=static_cast<int>(std::lround(getValueNormalized()*2.f));beginEdit();setValueNormalized(static_cast<float>((current+1)%3)*.5f);valueChanged();invalid();endEdit();return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}
void JerzyThreeWay::draw(CDrawContext* c){
    const auto& t=getJerzyGuiTheme();const CRect r(getViewSize());const auto p=r.getCenter();c->setDrawMode(kAntiAliasing|kNonIntegralMode);ellipse(c,p.x,p.y,15,CColor(91,80,63),t.edge);ellipse(c,p.x,p.y,11,CColor(8,18,18),t.text);const double end=p.y+11*(1.0-2.0*getValueNormalized());c->setLineWidth(7);c->setFrameColor(CColor(182,167,142));c->drawLine(p,{p.x,end});ellipse(c,p.x,end,6,CColor(221,207,179),t.edge);setDirty(false);
}
void JerzyLedToggle::draw(CDrawContext* c){JerzyToggle::draw(c);const auto& t=getJerzyGuiTheme();const auto r=getViewSize();const bool on=getValueNormalized()>=.5f;ellipse(c,r.left+8,r.top+8,5,on?t.ledOn:CColor(42,58,64),CColor(18,25,28));if(on)ellipse(c,r.left+8,r.top+8,2.4,CColor(255,255,210),CColor(255,255,210));setDirty(false);}
void JerzyLedThreeWay::draw(CDrawContext* c){JerzyThreeWay::draw(c);const auto& t=getJerzyGuiTheme();const auto r=getViewSize();const int pos=std::clamp((int)std::lround(getValueNormalized()*2.f),0,2);for(int i=0;i<3;++i)ellipse(c,r.left+8.0+i*10.0,r.top+8,3.6,i==pos?t.display:CColor(33,51,58),CColor(15,23,27));setDirty(false);}
void JerzyBypassButton::draw(CDrawContext* c){const auto& t=getJerzyGuiTheme();CRect r(getViewSize());const double x=r.getCenter().x,y=r.getCenter().y+7,rad=std::min(r.getWidth(),r.getHeight())*.29;c->setDrawMode(kAntiAliasing|kNonIntegralMode);const bool bypass=getValueNormalized()>=.5f;ellipse(c,x,r.top+9,7,CColor(85,41,15),t.edge);ellipse(c,x,r.top+9,4,bypass?t.accent:CColor(63,42,24),t.edge);metalKnob(c,{x-rad*1.25,y-rad*1.25,x+rad*1.25,y+rad*1.25},.5);setDirty(false);}

JerzyHardwarePanel::JerzyHardwarePanel(const CRect& r,IControlListener* l,int32_t tag):CKnob(r,l,tag,nullptr,nullptr){setMouseEnabled(false);}
void JerzyHardwarePanel::draw(CDrawContext* c){
    const auto& t=getJerzyGuiTheme();const CRect r(getViewSize());c->setDrawMode(kAntiAliasing|kNonIntegralMode);gradientRect(c,r,t.panelTop,t.panelBottom);c->setFrameColor(t.accent);c->setLineWidth(1.2);c->drawRect(r,kDrawStroked);CRect inner(r);inner.inset(4,4);c->setFrameColor(t.edge);c->setLineWidth(1);c->drawRect(inner,kDrawStroked);
    const CPoint screws[4]={{r.left+10,r.top+10},{r.right-10,r.top+10},{r.left+10,r.bottom-10},{r.right-10,r.bottom-10}};for(const auto& p:screws){ellipse(c,p.x+1,p.y+1,5.5,CColor(0,0,0,120),CColor(0,0,0,80));ellipse(c,p.x,p.y,4.8,CColor(114,113,100),CColor(210,198,166));c->setFrameColor(CColor(37,36,32));c->setLineWidth(1.2);c->drawLine({p.x-3,p.y+2},{p.x+3,p.y-2});}setDirty(false);
}
JerzyChassis::JerzyChassis(const CRect& r,IControlListener* l,int32_t tag):CKnob(r,l,tag,nullptr,nullptr){setMouseEnabled(false);}
void JerzyChassis::draw(CDrawContext* c){
    const auto& t=getJerzyGuiTheme();const CRect r(getViewSize());c->setDrawMode(kAntiAliasing|kNonIntegralMode);gradientRect(c,r,t.chassisTop,t.chassisBottom);
    for(int i=0;i<70;++i){const double y=r.top+(i+.5)*r.getHeight()/70.0;c->setFrameColor(i%3?CColor(t.text.red,t.text.green,t.text.blue,11):CColor(t.accent.red,t.accent.green,t.accent.blue,8));c->setLineWidth(.6);c->drawLine({r.left+4,y},{r.right-4,y});}
    for(int i=0;i<55;++i){const double fx=.5+.49*std::sin(5.71*i+.4),fy=.5+.48*std::sin(9.37*i+1.1),rr=.6+1.6*(.5+.5*std::sin(2.9*i));ellipse(c,r.left+fx*r.getWidth(),r.top+fy*r.getHeight(),rr,CColor(t.text.red,t.text.green,t.text.blue,18),CColor(0,0,0,0));}
    c->setFrameColor(CColor(t.text.red,t.text.green,t.text.blue,100));c->setLineWidth(2);c->drawRect(r,kDrawStroked);CRect inner(r);inner.inset(5,5);c->setFrameColor(CColor(7,18,20));c->setLineWidth(2);c->drawRect(inner,kDrawStroked);setDirty(false);
}
} // namespace JerzyAudio
