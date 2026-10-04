#pragma once
#include "tapedrive_params.h"
#include <algorithm>
#include <cmath>

namespace JerzyAudio {
// The original thirteen floats remain in the same order. New fields are appended.
// Keeping this codec shared prevents processor/controller state from drifting apart.
template<class Stream>
bool readTapeDriveState(Stream& stream, TapeDriveParams& p) {
    p=TapeDriveParams{};
    p.optoBypass=1.0; // Older sessions had no input compressor.
    double* values[]={&p.sat,&p.level,&p.dry,&p.gainMode,&p.shift,&p.bypass,
        &p.hpfCutoff,&p.hpfRes,&p.lpfCutoff,&p.lpfRes,&p.wowFlutter,
        &p.preampMode,&p.preampDrive,&p.flutter,&p.tapeAge,
        &p.optoAmount,&p.optoMakeup,&p.optoBypass};
    for(int i=0;i<18;++i){
        float value=0.0f;
        if(!stream.readFloat(value)) {
            if(i<6) return false;
            if(i<=13) p.flutter=0.25*p.wowFlutter;
            return true;
        }
        if(std::isfinite(value)) *values[i]=std::clamp(static_cast<double>(value),0.0,1.0);
    }
    return true;
}
template<class Stream>
bool writeTapeDriveState(Stream& stream,const TapeDriveParams& p) {
    const double values[]={p.sat,p.level,p.dry,p.gainMode,p.shift,p.bypass,
        p.hpfCutoff,p.hpfRes,p.lpfCutoff,p.lpfRes,p.wowFlutter,
        p.preampMode,p.preampDrive,p.flutter,p.tapeAge,
        p.optoAmount,p.optoMakeup,p.optoBypass};
    for(auto value:values) if(!stream.writeFloat(static_cast<float>(value))) return false;
    return true;
}
}
