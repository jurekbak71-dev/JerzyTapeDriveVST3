#pragma once
#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/ccolor.h"

namespace JerzyAudio {

struct JerzyGuiTheme {
    VSTGUI::CColor chassisTop;
    VSTGUI::CColor chassisBottom;
    VSTGUI::CColor panelTop;
    VSTGUI::CColor panelBottom;
    VSTGUI::CColor accent;
    VSTGUI::CColor text;
    VSTGUI::CColor edge;
    VSTGUI::CColor ledOn;
    VSTGUI::CColor display;
};

const JerzyGuiTheme& jerzyThemePetrol();
const JerzyGuiTheme& jerzyThemeBrass();
const JerzyGuiTheme& jerzyThemeSteel();
const JerzyGuiTheme& jerzyThemeBurgundy();
const JerzyGuiTheme& jerzyThemeViolet();

void setJerzyGuiTheme(const JerzyGuiTheme&);
const JerzyGuiTheme& getJerzyGuiTheme();

class JerzyKnob : public VSTGUI::CKnob {
public:
    JerzyKnob(const VSTGUI::CRect&,VSTGUI::IControlListener*,int32_t);
    void draw(VSTGUI::CDrawContext*) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint&,const VSTGUI::CButtonState&) override;
};

class JerzyAnalogMeter : public VSTGUI::CKnob {
public:
    JerzyAnalogMeter(const VSTGUI::CRect&,VSTGUI::IControlListener*,int32_t);
    void draw(VSTGUI::CDrawContext*) override;
};

class JerzyToggle : public VSTGUI::CKnob {
public:
    JerzyToggle(const VSTGUI::CRect&,VSTGUI::IControlListener*,int32_t);
    void draw(VSTGUI::CDrawContext*) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint&,const VSTGUI::CButtonState&) override;
};

class JerzyThreeWay : public JerzyToggle {
public:
    JerzyThreeWay(const VSTGUI::CRect&,VSTGUI::IControlListener*,int32_t);
    void draw(VSTGUI::CDrawContext*) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint&,const VSTGUI::CButtonState&) override;
};

class JerzyLedToggle : public JerzyToggle {
public:
    using JerzyToggle::JerzyToggle;
    void draw(VSTGUI::CDrawContext*) override;
};

class JerzyLedThreeWay : public JerzyThreeWay {
public:
    using JerzyThreeWay::JerzyThreeWay;
    void draw(VSTGUI::CDrawContext*) override;
};

class JerzyBypassButton : public JerzyToggle {
public:
    using JerzyToggle::JerzyToggle;
    void draw(VSTGUI::CDrawContext*) override;
};

class JerzyHardwarePanel : public VSTGUI::CKnob {
public:
    JerzyHardwarePanel(const VSTGUI::CRect&,VSTGUI::IControlListener*,int32_t);
    void draw(VSTGUI::CDrawContext*) override;
};

class JerzyChassis : public VSTGUI::CKnob {
public:
    JerzyChassis(const VSTGUI::CRect&,VSTGUI::IControlListener*,int32_t);
    void draw(VSTGUI::CDrawContext*) override;
};

} // namespace JerzyAudio
