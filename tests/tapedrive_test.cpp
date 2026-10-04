#include "../source/tapedrive_dsp.h"
#include "../source/tapedrive_state.h"
#include "../source/tapedrive_editor_geometry.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace JerzyAudio;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct FloatStream {
    std::vector<float> values;size_t at=0;
    bool readFloat(float& v){if(at==values.size())return false;v=values[at++];return true;}
    bool writeFloat(float v){values.push_back(v);return true;}
};
template<class T> struct Render {
    std::vector<T> left,right;
    double gr=0;
};
template<class T> Render<T> render(TapeDriveParams p,double sr,int block,bool silent=false){
    const int n=static_cast<int>(sr*3.0);
    std::vector<T> input(n);
    for(int i=0;i<n;++i) input[i]=silent?0:static_cast<T>(0.36*std::sin(6.283185307179586*997*i/sr));
    Render<T> result;result.left.resize(n);result.right.resize(n);
    TapeDriveDSP<T> dsp;dsp.prepare(sr,2);
    for(int i=0;i<n;i+=block){
        const T* in[]={input.data()+i,input.data()+i};T* out[]={result.left.data()+i,result.right.data()+i};
        double inPk,satPk,outPk,grPk;
        dsp.process(in,out,2,std::min(block,n-i),p,inPk,satPk,outPk,grPk);
        result.gr=std::max(result.gr,grPk);
    }
    for(auto v:result.left)require(std::isfinite(v)&&std::abs(v)<8,"Unstable left output");
    for(auto v:result.right)require(std::isfinite(v)&&std::abs(v)<8,"Unstable right output");
    return result;
}
template<class T> double energy(const std::vector<T>& x){double e=0;for(size_t i=x.size()/2;i<x.size();++i)e+=x[i]*x[i];return e/(x.size()-x.size()/2);}
template<class T> double difference(const std::vector<T>& a,const std::vector<T>& b){double e=0;for(size_t i=a.size()/2;i<a.size();++i){double d=a[i]-b[i];e+=d*d;}return e/(a.size()-a.size()/2);}
int main(){try{
    for(double dpi:{1.0,1.25,1.5,2.0})for(double zoom:{0.75,1.0,1.25,1.5}){
        const auto w=TapeDriveEditorGeometry::pixelWidth(zoom,dpi);
        const auto h=TapeDriveEditorGeometry::pixelHeight(zoom,dpi);
        require(std::abs(TapeDriveEditorGeometry::zoomForWidth(w,dpi)-zoom)<1e-6,"UI zoom must apply monitor DPI only once");
        require(std::abs(static_cast<double>(w)/h-1200.0/672.0)<0.002,"UI resize must preserve panel aspect ratio");
    }
    require(TapeDriveEditorGeometry::zoomForWidth(1,2)==0.75,"UI resize minimum ignored");
    require(TapeDriveEditorGeometry::zoomForWidth(10000,2)==1.5,"UI resize maximum ignored");
    TapeDriveParams p;p.optoBypass=1;p.sat=0.0;p.level=2.0/3.0;
    auto clean=render<double>(p,48000,128);
    auto silence=render<double>(p,48000,128,true);
    require(energy(silence.left)==0,"Age=0 must not generate hiss or cracks");
    require(difference(clean.left,clean.right)==0,"Clean stereo must be linked");
    p.wowFlutter=0.65;auto wow=render<double>(p,48000,128);
    p.wowFlutter=0;p.flutter=0.65;auto flutter=render<double>(p,48000,128);
    require(difference(wow.left,clean.left)>1e-4,"Wow must work independently");
    require(difference(flutter.left,clean.left)>1e-4,"Flutter must work independently");
    require(difference(wow.left,flutter.left)>1e-4,"Wow and Flutter must produce different modulation");
    require(difference(wow.left,wow.right)==0,"Transport drift must preserve stereo centre");
    p.wowFlutter=0.55;p.flutter=0.63;p.tapeAge=0.8;p.optoBypass=0;p.optoAmount=0.8;
    auto one=render<double>(p,48000,1),odd=render<double>(p,48000,127),large=render<double>(p,48000,4096);
    require(difference(one.left,odd.left)<1e-20&&difference(one.left,large.left)<1e-20,"DSP must not depend on host block size");
    auto wornSilence=render<double>(p,48000,127,true);
    require(energy(wornSilence.left)>1e-7,"Age must generate audible tape noise on silence");
    require(difference(wornSilence.left,wornSilence.right)>1e-7,"Tape noise must not be mono");
    p.wowFlutter=p.flutter=p.tapeAge=0;p.optoAmount=0.9;
    auto compressed=render<double>(p,48000,128);
    require(compressed.gr>0.1,"Optical cell must show gain reduction");
    require(energy(compressed.left)<energy(clean.left)*0.5,"Input opto must attenuate before tape");
    p.optoBypass=1;auto bypass=render<double>(p,48000,128);
    require(difference(clean.left,bypass.left)<1e-20,"Opto bypass must leave input unaffected");
    p.optoBypass=0;p.sat=1;p.gainMode=1;p.wowFlutter=p.flutter=p.tapeAge=1;
    p.hpfRes=p.lpfRes=1;
    for(double sr:{8000.,44100.,48000.,96000.,192000.})for(int mode=0;mode<3;++mode){
        p.preampMode=mode*0.5;render<float>(p,sr,511);render<double>(p,sr,511);
    }
    p.flutter=0.72;p.tapeAge=0.84;p.optoAmount=0.66;p.optoMakeup=0.73;p.optoBypass=0;
    FloatStream stream;require(writeTapeDriveState(stream,p),"State write failed");
    TapeDriveParams restored;require(readTapeDriveState(stream,restored),"State read failed");
    require(std::abs(restored.flutter-p.flutter)<1e-6&&std::abs(restored.tapeAge-p.tapeAge)<1e-6&&std::abs(restored.optoMakeup-p.optoMakeup)<1e-6,"New controls must round-trip");
    stream.values.resize(13);stream.at=0;require(readTapeDriveState(stream,restored),"Legacy state rejected");
    require(restored.optoBypass==1&&restored.tapeAge==0,"Legacy sessions must not enable new compression/noise");
    require(restored.flutter==0.25*restored.wowFlutter,"Legacy combined modulation must migrate");
    stream.values.resize(6);stream.at=0;require(readTapeDriveState(stream,restored),"Original six-float session rejected");
    stream.values[0]=std::numeric_limits<float>::quiet_NaN();stream.at=0;require(readTapeDriveState(stream,restored)&&std::isfinite(restored.sat),"Malformed session value must be sanitized");
    stream.values.resize(5);stream.at=0;require(!readTapeDriveState(stream,restored),"Truncated required state accepted");
    std::cout<<"Tape Drive: independent stochastic transport, wear, optical compression, block invariance, all sample rates/formats and legacy state OK\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
