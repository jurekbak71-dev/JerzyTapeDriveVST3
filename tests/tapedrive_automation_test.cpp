#include "../source/tapedrive_automation.h"
#include "../source/tapedrive_processor.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace JerzyAudio;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Point{int32 offset;double value;};
struct Queue: IParamValueQueue{
 ParamID id;std::vector<Point> points;std::vector<int> reads;
 Queue(ParamID id,std::initializer_list<Point> points):id(id),points(points),reads(points.size()){}
 tresult PLUGIN_API queryInterface(const TUID,void** obj)override{*obj=nullptr;return kNoInterface;}
 uint32 PLUGIN_API addRef()override{return 1;}
 uint32 PLUGIN_API release()override{return 1;}
 ParamID PLUGIN_API getParameterId()override{return id;}
 int32 PLUGIN_API getPointCount()override{return static_cast<int32>(points.size());}
 tresult PLUGIN_API getPoint(int32 i,int32& off,ParamValue& value)override{
  if(i<0 || i>=getPointCount())return kResultFalse;
  ++reads[i];off=points[i].offset;value=points[i].value;return kResultTrue;
 }
 tresult PLUGIN_API addPoint(int32 off,ParamValue value,int32& index)override{
  index=getPointCount();points.push_back({off,value});reads.push_back(0);return kResultTrue;
 }
 void allRead()const{for(auto n:reads)require(n==1,"Every automation point must be read exactly once");}
};
struct Changes:IParameterChanges{
 std::vector<Queue*> queues;
 Changes(std::initializer_list<Queue*> qs):queues(qs){}
 tresult PLUGIN_API queryInterface(const TUID,void** obj)override{*obj=nullptr;return kNoInterface;}
 uint32 PLUGIN_API addRef()override{return 1;}
 uint32 PLUGIN_API release()override{return 1;}
 int32 PLUGIN_API getParameterCount()override{return static_cast<int32>(queues.size());}
 IParamValueQueue* PLUGIN_API getParameterData(int32 i)override{return queues.at(i);}
 IParamValueQueue* PLUGIN_API addParameterData(const ParamID&,int32&)override{return nullptr;}
};
static void curveTests(){
 TapeDriveParams p;p.level=0;
 Queue level(kLevelId,{{3,1},{4,0},{7,0.6}}),bypass(kDriveBypassId,{{2,1},{6,0}});
 Queue unknown(9999,{{0,0.5},{5,1}});Changes changes{&bypass,&unknown,&level};
 const double expected[]={.25,.5,.75,1,0,.2,.4,.6};int samples=0;
 processTapeDriveAutomation(&changes,p,8,[&](int32 start,int32 n){
  for(int i=start;i<start+n;++i){
   require(std::abs(p.level-expected[i])<1e-12,"VST3 ramp/jump interpolation incorrect");
   require(p.bypass==double(i>=2 && i<6),"Discrete switch applied at wrong sample");++samples;
  }
 });
 require(samples==8,"Sample range incomplete");level.allRead();bypass.allRead();unknown.allRead();
 // The implicit point at -1 must produce the same ramp across host blocks.
 TapeDriveParams whole,split;whole.level=split.level=0;
 double a[32]{},b[32]{};Queue full(kLevelId,{{31,1}});Changes fullChanges{&full};
 processTapeDriveAutomation(&fullChanges,whole,32,[&](int32 start,int32 n){
  for(int i=start;i<start+n;++i)a[i]=whole.level;
 });
 for(int block=0;block<4;++block){
  Queue part(kLevelId,{{7,(block+1)/4.0}});Changes partChanges{&part};
  processTapeDriveAutomation(&partChanges,split,8,[&](int32 start,int32 n){
   for(int i=start;i<start+n;++i)b[block*8+i]=split.level;
  });part.allRead();
 }
 for(int i=0;i<32;++i)require(std::abs(a[i]-b[i])<1e-12,"Ramp depends on host block boundaries");
 full.allRead();
 int calls=0;processTapeDriveAutomation(nullptr,p,1024,[&](int32 start,int32 n){
  require(start==0 && n==1024,"No automation must keep full-block rendering");++calls;
 });require(calls==1,"No automation must render once");
 Queue invalid(kSatId,{{-1,1},{0,std::numeric_limits<double>::quiet_NaN()},
   {2,2},{1,.1},{8,0},{7,-1}});Changes bad{&invalid};
 processTapeDriveAutomation(&bad,p,8,[&](int32,int32){require(std::isfinite(p.sat),"NaN reached DSP");});
 require(p.sat==0,"Out-of-range value was not clamped");invalid.allRead();
 Queue flush(kDryId,{{0,.2},{0,.8}});Changes noAudio{&flush};
 processTapeDriveAutomation(&noAudio,p,0,[&](int32,int32){throw std::runtime_error("Flush rendered audio");});
 require(p.dry==.8,"Flush must preserve final parameter state");flush.allRead();
}
template<class T> static void processorTests(int channels,bool inPlace){
 constexpr int n=32;
 std::vector<T> input[2],actual[2],reference[2];
 for(int c=0;c<channels;++c){
  input[c].resize(n);actual[c].resize(n);reference[c].resize(n);
  for(int i=0;i<n;++i)input[c][i]=T(.2*std::sin(.27*i+.1*c));
  if(inPlace)actual[c]=input[c];
 }
 ProcessSetup setup{};setup.sampleRate=48000;setup.maxSamplesPerBlock=n;
 setup.symbolicSampleSize=std::is_same<T,float>::value?kSample32:kSample64;
 TapeDriveProcessor processor;require(processor.setupProcessing(setup)==kResultOk,"Setup failed");
 // Interleaved queues, two bypass changes inside the block, continuous ramps,
 // first/last sample events and unknown/read-only parameters.
 Queue level(kLevelId,{{0,.3},{9,.8},{10,.1},{31,.7}});
 Queue dry(kDryId,{{4,.6},{20,.1}}),bp(kDriveBypassId,{{7,1},{17,0}});
 Queue wear(kTapeAgeId,{{12,.9},{31,.4}}),meter(kInputMeterId,{{0,1},{2,0}});
 Changes changes{&wear,&bp,&meter,&dry,&level};
 T* in[2]{};T* out[2]{};
 for(int c=0;c<channels;++c){in[c]=inPlace?actual[c].data():input[c].data();out[c]=actual[c].data();}
 AudioBusBuffers inputs{},outputs{};inputs.numChannels=outputs.numChannels=channels;
 if constexpr(std::is_same<T,float>::value){inputs.channelBuffers32=in;outputs.channelBuffers32=out;}
 else {inputs.channelBuffers64=in;outputs.channelBuffers64=out;}
 ProcessData d{};d.numSamples=n;d.symbolicSampleSize=setup.symbolicSampleSize;
 d.numInputs=d.numOutputs=1;d.inputs=&inputs;d.outputs=&outputs;d.inputParameterChanges=&changes;
 require(processor.process(d)==kResultOk,"Automated processor failed");
 level.allRead();dry.allRead();bp.allRead();wear.allRead();meter.allRead();
 // Independent sample-by-sample oracle, with explicit expected curves.
 TapeDriveDSP<T> dsp;dsp.prepare(48000,2);TapeDriveParams p;
 for(int i=0;i<n;++i){
  p.level=i<=9?.3+(.8-.3)*i/9.0:i==10?.1:.1+(.7-.1)*(i-10)/21.0;
  p.dry=i<=4?.6*(i+1)/5.0:i<=20?.6+(.1-.6)*(i-4)/16.0:.1;
  p.tapeAge=i<=12?.9*(i+1)/13.0:.9+(.4-.9)*(i-12)/19.0;
  p.bypass=i>=7 && i<17?1:0;
  const T* ri[2]{};T* ro[2]{};
  for(int c=0;c<channels;++c){ri[c]=input[c].data()+i;ro[c]=reference[c].data()+i;}
  if(p.bypass)for(int c=0;c<channels;++c)*ro[c]=*ri[c];
  else {double a,b,e,f;dsp.process(ri,ro,channels,1,p,a,b,e,f);}
 }
 for(int c=0;c<channels;++c)for(int i=0;i<n;++i)
  require(std::abs(double(actual[c][i])-double(reference[c][i]))<1e-6,"Processor audio differs from timestamped sample oracle");
 // No-input/zero-sample calls must consume queues safely too.
 Queue flush(kLevelId,{{0,.5},{0,.8}});Changes flushChanges{&flush};
 d.numSamples=0;d.numInputs=d.numOutputs=0;d.inputParameterChanges=&flushChanges;
 require(processor.process(d)==kResultOk,"Processor flush failed");flush.allRead();
}
int main(){try{
 curveTests();for(int channels:{1,2})for(bool inPlace:{false,true}){
  processorTests<float>(channels,inPlace);processorTests<double>(channels,inPlace);
 }
 std::cout<<"TapeDrive automation: all points consumed, VST3 ramps/jumps, discrete sample offsets, mono/stereo float/double in-place audio and flush OK\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
