#include "tapedrive_gui_views.h"
#include "tapedrive_params.h"
#include "vstgui/uidescription/uiviewfactory.h"
#include "vstgui/uidescription/uiviewcreator.h"
#include "vstgui/uidescription/iviewcreator.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cgradient.h"
#include "vstgui/lib/cfont.h"
#include <algorithm>
#include <cmath>

using namespace VSTGUI;
namespace JerzyAudio {
namespace {
const CColor amber(230,160,78),cream(236,217,181),black(15,14,12),edge(133,112,80);
void ellipse(CDrawContext* c,double x,double y,double radius,CColor fill,CColor frame){
    c->setFillColor(fill); c->setFrameColor(frame); c->setLineWidth(1.0);
    c->drawEllipse({x-radius,y-radius,x+radius,y+radius},kDrawFilledAndStroked);
}
void gradientRect(CDrawContext* c,const CRect& r,CColor a,CColor b){
    auto* path=c->createGraphicsPath();
    if(!path)return;
    path->addRect(r);
    auto* gradient=CGradient::create(0,1,a,b);
    if(gradient){ c->fillLinearGradient(path,*gradient,{r.left,r.top},{r.left,r.bottom}); gradient->forget(); }
    path->forget();
}
void text(CDrawContext* c,const char* s,const CRect& r,double size,CColor color){
    c->setFont(kNormalFont,size); c->setFontColor(color); c->drawString(s,r,kCenterText);
}
void metalKnob(CDrawContext* c,const CRect& r,double value){
    const double x=r.getCenter().x,y=r.getCenter().y;
    const double radius=std::min(r.getWidth(),r.getHeight())*0.39;
    c->setDrawMode(kAntiAliasing|kNonIntegralMode);
    for(int i=0;i<=10;++i){
        const double a=(135.0+27.0*i)*Constants::pi/180.0;
        c->setFrameColor(amber); c->setLineWidth(i==5?2.2:1.5);
        c->drawLine({x+std::cos(a)*radius*1.11,y+std::sin(a)*radius*1.11},
                    {x+std::cos(a)*radius*1.27,y+std::sin(a)*radius*1.27});
    }
    ellipse(c,x+2,y+4,radius+2,CColor(0,0,0,130),black);
    ellipse(c,x,y,radius,CColor(43,39,33),edge);
    // Rubber grip scallops surrounding a spun-metal centre.
    for(int i=0;i<18;++i){
        const double a=2.0*Constants::pi*i/18.0;
        ellipse(c,x+radius*0.82*std::cos(a),y+radius*0.82*std::sin(a),radius*0.18,CColor(23,22,20),CColor(48,44,39));
    }
    ellipse(c,x,y,radius*0.76,CColor(103,93,79),CColor(186,171,146));
    CDrawContext::PointList wedge;
    const double metalRadius=radius*0.70;
    for(int i=0;i<120;++i){
        const double a=2.0*Constants::pi*i/120.0;
        const double next=a+2.0*Constants::pi/120.0;
        const auto shade=static_cast<uint8_t>(std::clamp(130.0+55.0*std::cos(2.0*a+0.6)+15.0*std::cos(6.0*a),55.0,215.0));
        c->setFillColor(CColor(shade,static_cast<uint8_t>(shade*0.96),static_cast<uint8_t>(shade*0.88)));
        wedge={{x,y},{x+metalRadius*std::cos(a),y+metalRadius*std::sin(a)},
                       {x+metalRadius*std::cos(next),y+metalRadius*std::sin(next)}};
        c->drawPolygon(wedge,kDrawFilled);
    }
    for(int i=1;i<8;++i){
        c->setFrameColor(CColor(235,222,200,22)); c->setLineWidth(0.5);
        const double rr=metalRadius*i/8.0;
        c->drawEllipse({x-rr,y-rr,x+rr,y+rr},kDrawStroked);
    }
    const double a=(135.0+270.0*std::clamp(value,0.0,1.0))*Constants::pi/180.0;
    c->setFrameColor(cream); c->setLineWidth(std::max(2.0,radius*0.065));
    c->drawLine({x+radius*0.61*std::cos(a),y+radius*0.61*std::sin(a)},
                {x+radius*0.98*std::cos(a),y+radius*0.98*std::sin(a)});
}
}

ChickenKnob::ChickenKnob(const CRect& r,IControlListener* l,int32_t tag):CKnob(r,l,tag,nullptr,nullptr){
    setStartAngle(static_cast<float>(Constants::pi*0.75));
    setRangeAngle(static_cast<float>(Constants::pi*1.5));
    setWheelInc(0.01f);
}
void ChickenKnob::draw(CDrawContext* c){metalKnob(c,getViewSize(),getValueNormalized());setDirty(false);}
CMouseEventResult ChickenKnob::onMouseDown(CPoint& where,const CButtonState& buttons){
    if(buttons.isRightButton()){
        beginEdit();setValue(getDefaultValue());valueChanged();invalid();endEdit();
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }
    return CKnob::onMouseDown(where,buttons);
}

AnalogMeter::AnalogMeter(const CRect& r,IControlListener* l,int32_t tag):CKnob(r,l,tag,nullptr,nullptr){
    setMouseEnabled(false);
}
void AnalogMeter::draw(CDrawContext* c){
    const CRect r(getViewSize());
    c->setDrawMode(kAntiAliasing|kNonIntegralMode);
    c->setFillColor(black);c->setFrameColor(edge);c->setLineWidth(2);
    c->drawRect(r,kDrawFilledAndStroked);
    CRect face(r);face.inset(6,6);
    gradientRect(c,face,CColor(157,90,35),CColor(255,225,154));
    const double cx=face.getCenter().x,cy=face.bottom-7;
    const double radius=std::min(face.getWidth()*0.48,face.getHeight()*0.86);
    for(int i=0;i<=10;++i){
        const double a=(210+12*i)*Constants::pi/180.0;
        c->setFrameColor(i>=8?CColor(191,47,25):CColor(61,37,18));c->setLineWidth(i%2?1:1.6);
        c->drawLine({cx+radius*0.77*std::cos(a),cy+radius*0.77*std::sin(a)},
                    {cx+radius*0.94*std::cos(a),cy+radius*0.94*std::sin(a)});
    }
    text(c,"-20     -10      -3      0   +3",{face.left,face.top+6,face.right,face.top+19},9,CColor(60,34,14));
    text(c,"VU",{face.left,face.bottom-27,face.right,face.bottom-11},13,CColor(60,34,14));
    const double a=(210+120*std::clamp(static_cast<double>(getValueNormalized()),0.0,1.0))*Constants::pi/180.0;
    c->setFrameColor(CColor(81,27,15));c->setLineWidth(2);
    c->drawLine({cx,cy},{cx+radius*0.87*std::cos(a),cy+radius*0.87*std::sin(a)});
    ellipse(c,cx,cy,3,CColor(63,38,18),CColor(63,38,18));setDirty(false);
}

ToggleSwitch::ToggleSwitch(const CRect& r,IControlListener* l,int32_t tag):CKnob(r,l,tag,nullptr,nullptr){setMin(0);setMax(1);setWheelInc(1);}
CMouseEventResult ToggleSwitch::onMouseDown(CPoint&,const CButtonState& buttons){
    if(buttons.isRightButton()){
        beginEdit();setValue(getDefaultValue());valueChanged();invalid();endEdit();
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }
    if(!buttons.isLeftButton())return kMouseEventNotHandled;
    beginEdit();setValueNormalized(getValueNormalized()<0.5f?1.f:0.f);valueChanged();invalid();endEdit();
    return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}
void ToggleSwitch::draw(CDrawContext* c){
    const CRect r(getViewSize());const auto p=r.getCenter();
    c->setDrawMode(kAntiAliasing|kNonIntegralMode);
    ellipse(c,p.x,p.y,15,CColor(91,80,63),edge);
    ellipse(c,p.x,p.y,11,black,cream);
    const double end=p.y+(getValueNormalized()>=0.5f?-11:11);
    c->setLineWidth(7);c->setFrameColor(CColor(182,167,142));c->drawLine(p,{p.x,end});
    ellipse(c,p.x,end,6,CColor(221,207,179),edge);setDirty(false);
}
ThreeWaySwitch::ThreeWaySwitch(const CRect& r,IControlListener* l,int32_t tag):ToggleSwitch(r,l,tag){setMax(2);}
CMouseEventResult ThreeWaySwitch::onMouseDown(CPoint& where,const CButtonState& buttons){
    if(buttons.isRightButton())return ToggleSwitch::onMouseDown(where,buttons);
    if(!buttons.isLeftButton())return kMouseEventNotHandled;
    const int current=static_cast<int>(std::lround(getValueNormalized()*2.f));
    beginEdit();setValueNormalized(static_cast<float>((current+1)%3)*0.5f);valueChanged();invalid();endEdit();
    return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}
void ThreeWaySwitch::draw(CDrawContext* c){
    const CRect r(getViewSize());const auto p=r.getCenter();
    c->setDrawMode(kAntiAliasing|kNonIntegralMode);
    ellipse(c,p.x,p.y,15,CColor(91,80,63),edge);ellipse(c,p.x,p.y,11,black,cream);
    const double end=p.y+11*(1.0-2.0*getValueNormalized());
    c->setLineWidth(7);c->setFrameColor(CColor(182,167,142));c->drawLine(p,{p.x,end});
    ellipse(c,p.x,end,6,CColor(221,207,179),edge);setDirty(false);
}
void LedToggleSwitch::draw(CDrawContext* c){
    ToggleSwitch::draw(c);
    const auto r=getViewSize();const bool on=getValueNormalized()>=0.5f;
    ellipse(c,r.left+8,r.top+8,5,on?CColor(255,194,73):CColor(42,58,64),CColor(18,25,28));
    if(on)ellipse(c,r.left+8,r.top+8,2.4,CColor(255,236,166),CColor(255,236,166));
    setDirty(false);
}
void LedThreeWaySwitch::draw(CDrawContext* c){
    ThreeWaySwitch::draw(c);
    const auto r=getViewSize();const int pos=std::clamp((int)std::lround(getValueNormalized()*2.f),0,2);
    for(int i=0;i<3;++i){
        const double x=r.left+8.0+i*10.0;
        ellipse(c,x,r.top+8,3.6,i==pos?CColor(92,220,235):CColor(33,51,58),CColor(15,23,27));
    }
    setDirty(false);
}
void BypassButton::draw(CDrawContext* c){
    CRect r(getViewSize());const double x=r.getCenter().x,y=r.getCenter().y+7;
    const double radius=std::min(r.getWidth(),r.getHeight())*0.29;
    c->setDrawMode(kAntiAliasing|kNonIntegralMode);
    const bool bypass=getValueNormalized()>=0.5f;
    ellipse(c,x,r.top+9,7,CColor(85,41,15),edge);
    ellipse(c,x,r.top+9,4,bypass?CColor(255,193,71):CColor(63,42,24),edge);
    metalKnob(c,{x-radius*1.25,y-radius*1.25,x+radius*1.25,y+radius*1.25},0.5);
    setDirty(false);
}
HardwarePanel::HardwarePanel(const CRect& r,IControlListener* l,int32_t tag):CKnob(r,l,tag,nullptr,nullptr){setMouseEnabled(false);}
void HardwarePanel::draw(CDrawContext* c){
    c->setDrawMode(kAntiAliasing|kNonIntegralMode);
    c->setFrameColor(CColor(169,113,58,130));c->setLineWidth(1);
    c->drawLine({40,397},{720,397});c->drawLine({735,235},{735,547});
    c->drawLine({40,551},{1160,551});c->drawLine({760,381},{1160,381});
    setDirty(false);
}
OxidizedPanel::OxidizedPanel(const CRect& r,IControlListener* l,int32_t tag):CKnob(r,l,tag,nullptr,nullptr){setMouseEnabled(false);}
void OxidizedPanel::draw(CDrawContext* c){
    const CRect r(getViewSize());
    c->setDrawMode(kAntiAliasing|kNonIntegralMode);
    gradientRect(c,r,CColor(19,62,79),CColor(8,28,42));
    // Deterministic blue-oxidized steel patina: broad blooms, pitting and brushed scratches.
    for(int i=0;i<42;++i){
        const double fx=0.5+0.47*std::sin(1.713*i+0.4);
        const double fy=0.5+0.46*std::sin(2.371*i+1.1);
        const double rr=22.0+70.0*(0.5+0.5*std::sin(3.117*i));
        const uint8_t a=static_cast<uint8_t>(18+22*(0.5+0.5*std::sin(0.87*i)));
        ellipse(c,r.left+fx*r.getWidth(),r.top+fy*r.getHeight(),rr,CColor(65,143,157,a),CColor(13,63,79,18));
    }
    for(int i=0;i<150;++i){
        const double fx=0.5+0.49*std::sin(7.13*i+0.2);
        const double fy=0.5+0.49*std::sin(11.71*i+1.8);
        const double rr=0.7+2.4*(0.5+0.5*std::sin(5.17*i));
        ellipse(c,r.left+fx*r.getWidth(),r.top+fy*r.getHeight(),rr,CColor(108,173,181,38),CColor(5,35,49,25));
    }
    for(int i=0;i<34;++i){
        const double y=r.top+(i+0.5)*r.getHeight()/34.0;
        const double dx=8.0+34.0*(0.5+0.5*std::sin(i*1.9));
        c->setFrameColor(CColor(170,211,216,18));c->setLineWidth(0.7);
        c->drawLine({r.left+dx,y},{r.right-dx*.7,y+std::sin(i*.8)*2.0});
    }
    c->setFrameColor(CColor(132,185,194,90));c->setLineWidth(2.0);c->drawRect(r,kDrawStroked);
    setDirty(false);
}
namespace {
template<class T> class SimpleCreator : public ViewCreatorAdapter {
public:
    SimpleCreator(const char* name):name(name){UIViewFactory::registerViewCreator(*this);}
    IdStringPtr getViewName() const override{return name;}
    IdStringPtr getBaseViewName() const override{return "CKnob";}
    CView* create(const UIAttributes&,const IUIDescription*) const override{return new T(CRect(0,0,100,100),nullptr,-1);}
private:const char* name;
};
SimpleCreator<ChickenKnob> knob("ChickenKnob");SimpleCreator<AnalogMeter> meter("AnalogMeter");
SimpleCreator<ToggleSwitch> toggle("ToggleSwitch");SimpleCreator<ThreeWaySwitch> three("ThreeWaySwitch");
SimpleCreator<LedToggleSwitch> ledToggle("LedToggleSwitch");SimpleCreator<LedThreeWaySwitch> ledThree("LedThreeWaySwitch");
SimpleCreator<BypassButton> bypass("BypassButton");SimpleCreator<HardwarePanel> panel("HardwarePanel");SimpleCreator<OxidizedPanel> oxidePanel("OxidizedPanel");
}
void registerTapeDriveViews(){}
}
