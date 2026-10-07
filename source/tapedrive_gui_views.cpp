#include "tapedrive_gui_views.h"
#include "vstgui/uidescription/uiviewfactory.h"
#include "vstgui/uidescription/uiviewcreator.h"
#include "vstgui/uidescription/iviewcreator.h"

using namespace VSTGUI;
namespace JerzyAudio {
namespace {
template<class T> class SimpleCreator : public ViewCreatorAdapter {
public:
    explicit SimpleCreator(const char* n):name(n){UIViewFactory::registerViewCreator(*this);}
    IdStringPtr getViewName() const override{return name;}
    IdStringPtr getBaseViewName() const override{return "CKnob";}
    CView* create(const UIAttributes&,const IUIDescription*) const override{return new T(CRect(0,0,100,100),nullptr,-1);}
private:const char* name;
};
SimpleCreator<ChickenKnob> knob("ChickenKnob");
SimpleCreator<AnalogMeter> meter("AnalogMeter");
SimpleCreator<ToggleSwitch> toggle("ToggleSwitch");
SimpleCreator<ThreeWaySwitch> three("ThreeWaySwitch");
SimpleCreator<LedToggleSwitch> ledToggle("LedToggleSwitch");
SimpleCreator<LedThreeWaySwitch> ledThree("LedThreeWaySwitch");
SimpleCreator<BypassButton> bypass("BypassButton");
SimpleCreator<HardwarePanel> panel("HardwarePanel");
SimpleCreator<OxidizedPanel> oxidePanel("OxidizedPanel");
}
void registerTapeDriveViews(){ setJerzyGuiTheme(jerzyThemeBrass()); }
} // namespace JerzyAudio
