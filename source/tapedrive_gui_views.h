#pragma once
#include "jerzy_vst_gui_kit.h"

namespace JerzyAudio {

using ChickenKnob = JerzyKnob;
using AnalogMeter = JerzyAnalogMeter;
using ToggleSwitch = JerzyToggle;
using ThreeWaySwitch = JerzyThreeWay;
using LedToggleSwitch = JerzyLedToggle;
using LedThreeWaySwitch = JerzyLedThreeWay;
using BypassButton = JerzyBypassButton;
using HardwarePanel = JerzyHardwarePanel;
using OxidizedPanel = JerzyChassis;

void registerTapeDriveViews();

} // namespace JerzyAudio
