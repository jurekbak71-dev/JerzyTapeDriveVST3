#pragma once
#include "tapedrive_params.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace JerzyAudio {
// One cursor per writable parameter; queues stay owned by the host. No heap
// allocation, sorting or locks in the audio callback.
struct TapeDriveAutomationCursor {
    double TapeDriveParams::* field;
    bool discrete;
    Steinberg::Vst::IParamValueQueue* queue=nullptr;
    Steinberg::int32 index=0,points=0,previousOffset=-1,nextOffset=0;
    double previousValue=0,nextValue=0;
    bool pending=false;

    void advance(Steinberg::int32 samples) {
        pending=false;
        while(index<points){
            Steinberg::int32 offset=0;double value=0;
            const auto result=queue->getPoint(index++,offset,value);
            if(result!=Steinberg::kResultTrue || !std::isfinite(value))continue;
            // Invalid or out-of-order host points are consumed, never rendered.
            if(samples>0 && (offset<0 || offset>=samples || offset<previousOffset))continue;
            nextOffset=offset;nextValue=std::clamp(value,0.0,1.0);pending=true;return;
        }
    }
};

template<class Render>
void processTapeDriveAutomation(Steinberg::Vst::IParameterChanges* changes,
                               TapeDriveParams& p,Steinberg::int32 samples,Render render){
    using Cursor=TapeDriveAutomationCursor;
    std::array<Cursor,18> cursors{{
        {&TapeDriveParams::sat,false},{&TapeDriveParams::level,false},
        {&TapeDriveParams::dry,false},{&TapeDriveParams::gainMode,true},
        {&TapeDriveParams::shift,true},{&TapeDriveParams::bypass,true},
        {&TapeDriveParams::hpfCutoff,false},{&TapeDriveParams::hpfRes,false},
        {&TapeDriveParams::lpfCutoff,false},{&TapeDriveParams::lpfRes,false},
        {&TapeDriveParams::wowFlutter,false},{&TapeDriveParams::preampMode,true},
        {&TapeDriveParams::preampDrive,false},{&TapeDriveParams::flutter,false},
        {&TapeDriveParams::tapeAge,false},{&TapeDriveParams::optoAmount,false},
        {&TapeDriveParams::optoMakeup,false},{&TapeDriveParams::optoBypass,true}
    }};
    constexpr Steinberg::Vst::ParamID ids[]={kSatId,kLevelId,kDryId,kGainModeId,
        kShiftId,kDriveBypassId,kHPFCutoffId,kHPFResId,kLPFCutoffId,kLPFResId,
        kWowFlutterId,kPreampModeId,kPreampDriveId,kFlutterId,kTapeAgeId,
        kOptoAmountId,kOptoMakeupId,kOptoBypassId};
    if(changes)for(Steinberg::int32 i=0;i<changes->getParameterCount();++i){
        auto* queue=changes->getParameterData(i);if(!queue)continue;
        const auto id=queue->getParameterId();size_t slot=0;
        while(slot<cursors.size() && ids[slot]!=id)++slot;
        if(slot==cursors.size() || cursors[slot].queue){
            // Read-only meters, unknown IDs and duplicate queues cannot alter DSP.
            for(Steinberg::int32 j=0;j<queue->getPointCount();++j){
                Steinberg::int32 offset;double value;queue->getPoint(j,offset,value);
            }
            continue;
        }
        auto& c=cursors[slot];c.queue=queue;c.points=queue->getPointCount();
        c.previousValue=p.*c.field;c.advance(samples);
    }
    if(samples<=0){
        for(auto& c:cursors)while(c.pending){p.*c.field=c.nextValue;c.advance(samples);}
        return;
    }
    for(Steinberg::int32 sample=0;sample<samples;){
        auto end=samples;
        for(auto& c:cursors){
            while(c.pending && c.nextOffset<=sample){
                c.previousOffset=c.nextOffset;c.previousValue=c.nextValue;
                p.*c.field=c.nextValue;c.advance(samples);
            }
            if(!c.pending)continue;
            if(!c.discrete && c.nextValue!=c.previousValue){
                // VST3's implicit start point is the last value at offset -1.
                const double t=double(sample-c.previousOffset)/double(c.nextOffset-c.previousOffset);
                p.*c.field=c.previousValue+(c.nextValue-c.previousValue)*t;
                end=sample+1;
            }else end=std::min(end,c.nextOffset);
        }
        render(sample,end-sample);sample=end;
    }
}
}
