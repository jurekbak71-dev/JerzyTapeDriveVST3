#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdint>
#include "tapedrive_params.h"

namespace JerzyAudio {

template <typename Sample>
class TapeDriveDSP {
public:
    void prepare(double sr, int channels=2) {
        sampleRate=std::max(8000.0,sr);
        numChannels=std::clamp(channels,1,2);
        const size_t delaySize=static_cast<size_t>(sampleRate*0.040)+8;
        for(int ch=0;ch<2;++ch){
            wowBuffer[ch].assign(delaySize,0.0);
            wowWrite[ch]=0;
        }
        reset();
    }

    void reset(){
        for(int i=0;i<2;++i){
            low[i]=0.0; env[i]=0.0; dc[i]=0.0;
            hp_z1[i]=hp_z2[i]=0.0;
            lp_z1[i]=lp_z2[i]=0.0;
            wearLP[i]=0.0;
            tubeSag[i]=0.0;
            tubeTone[i]=0.0;
            transistorTone[i]=0.0;
            std::fill(wowBuffer[i].begin(),wowBuffer[i].end(),0.0);
            wowWrite[i]=0;
        }
        randomState=0x4a65727au;
        wowMotion={}; flutterMotion={};
        smoothWow=smoothFlutter=smoothAge=0.0;
        optoFast=optoSlow=optoReduction=0.0;
        smoothOpto=0.0; smoothMakeup=1.0; smoothOptoMix=0.0;
        for(int ch=0;ch<2;++ch){
            hissLP[ch]=crackle[ch]=dropout[ch]=dropoutTarget[ch]=0.0;
            dropoutLeft[ch]=0;
        }
    }

    void process(const Sample* const* in, Sample* const* out, int channels, int n,
                 const TapeDriveParams& p,
                 double& inputPeak, double& saturationPeak, double& outputPeak,
                 double& reductionPeak) {
        double sat=p.sat, levelNorm=p.level, dryNorm=p.dry;
        const int gainMode=p.gainMode>=0.5?1:0;
        const int shiftMode=std::clamp(static_cast<int>(std::lround(p.shift*2.0)),0,2);
        int preampMode=std::clamp(static_cast<int>(std::lround(p.preampMode*2.0)),0,2);
        double preampDriveNorm=p.preampDrive;
        const double wowTarget=std::clamp(p.wowFlutter,0.0,1.0);
        const double flutterTarget=std::clamp(p.flutter,0.0,1.0);
        const double ageTarget=std::clamp(p.tapeAge,0.0,1.0);
        const double optoTarget=std::clamp(p.optoAmount,0.0,1.0);
        const double makeupTarget=p.optoBypass>=0.5?1.0:std::pow(10.0,(-12.0+24.0*std::clamp(p.optoMakeup,0.0,1.0))/20.0);
        channels=std::clamp(channels,1,numChannels);
        sat=std::clamp(sat,0.0,1.0);
        levelNorm=std::clamp(levelNorm,0.0,1.0);
        dryNorm=std::clamp(dryNorm,0.0,1.0);

        preampDriveNorm=std::clamp(preampDriveNorm,0.0,1.0);
        preampMode=std::clamp(preampMode,0,2);

        inputPeak=0.0; saturationPeak=0.0; outputPeak=0.0; reductionPeak=0.0;

        const double driveDb = (gainMode ? 12.0 + 30.0*sat : 22.0*sat);
        const double drive = std::pow(10.0,driveDb/20.0);
        const double outDb = -24.0 + 36.0*levelNorm;
        const double outGain = std::pow(10.0,outDb/20.0);
        const double dryGain = dryNorm <= 0.001 ? 0.0 : std::pow(10.0,(6.0*dryNorm)/20.0)*dryNorm;
        const double lowA = std::exp(-2.0*3.141592653589793*2200.0/sampleRate);
        const double atk = std::exp(-1.0/(0.006*sampleRate));
        const double rel = std::exp(-1.0/(0.090*sampleRate));
        const double dcA = std::exp(-2.0*3.141592653589793*18.0/sampleRate);

        Biquad hp = makeHighPass(mapLog(p.hpfCutoff,20.0,2000.0),mapQ(p.hpfRes));
        Biquad lp = makeLowPass(mapLog(p.lpfCutoff,1000.0,20000.0),mapQ(p.lpfRes));

        const double smoothA=std::exp(-1.0/(0.025*sampleRate));
        const double optoAttack=std::exp(-1.0/(0.012*sampleRate));
        const double optoRelease=std::exp(-1.0/(0.090*sampleRate));
        const double optoMemory=std::exp(-1.0/(1.3*sampleRate));
        const double hissA=std::exp(-2.0*3.141592653589793*1800.0/sampleRate);
        const double crackA=std::exp(-1.0/(0.0015*sampleRate));
        const double dropAttack=std::exp(-1.0/(0.004*sampleRate));
        const double dropRelease=std::exp(-1.0/(0.030*sampleRate));

        const double tubeDrive=std::pow(10.0,(4.0+30.0*preampDriveNorm)/20.0);
        const double transistorDrive=std::pow(10.0,(2.0+34.0*preampDriveNorm)/20.0);
        const double tubeSagAtk=std::exp(-1.0/(0.018*sampleRate));
        const double tubeToneA=std::exp(-2.0*3.141592653589793*(11500.0-3500.0*preampDriveNorm)/sampleRate);
        const double transistorToneA=std::exp(-2.0*3.141592653589793*4200.0/sampleRate);

        for(int i=0;i<n;++i){
            smoothWow=smoothA*smoothWow+(1.0-smoothA)*wowTarget;
            smoothFlutter=smoothA*smoothFlutter+(1.0-smoothA)*flutterTarget;
            smoothAge=smoothA*smoothAge+(1.0-smoothA)*ageTarget;
            smoothOpto=smoothA*smoothOpto+(1.0-smoothA)*optoTarget;
            smoothMakeup=smoothA*smoothMakeup+(1.0-smoothA)*makeupTarget;
            smoothOptoMix=smoothA*smoothOptoMix+(1.0-smoothA)*(p.optoBypass>=0.5?0.0:1.0);
            // Random-duration cubic trajectories: no repeating LFO waveform.
            const double wow=nextMotion(wowMotion,0.25,1.65);
            const double flutter=nextMotion(flutterMotion,0.012,0.060);
            const double movement=std::max(smoothWow,smoothFlutter);
            const double baseDelayMs=4.0*std::min(1.0,movement*1000.0);
            const double delaySamples=(baseDelayMs+2.7*smoothWow*wow+0.22*smoothFlutter*flutter)*0.001*sampleRate;
            const double wearCutoff=18000.0-12500.0*smoothAge;
            const double wearA=std::exp(-2.0*3.141592653589793*wearCutoff/sampleRate);
            // Stereo-linked optical cell with a slow memory and a two-stage recovery.
            double detector=0.0;
            for(int ch=0;ch<channels;++ch)
                if(in && in[ch]) detector=std::max(detector,std::abs(static_cast<double>(in[ch][i])));
            const double optoCoeff=detector>optoFast?optoAttack:optoRelease;
            optoFast=optoCoeff*optoFast+(1.0-optoCoeff)*detector;
            optoSlow=optoMemory*optoSlow+(1.0-optoMemory)*optoFast;
            const double detectorDb=20.0*std::log10(std::max(1e-9,0.72*optoFast+0.28*optoSlow));
            const double over=detectorDb-(-10.0-20.0*smoothOpto);
            const double knee=6.0;
            const double kneeOver=over<=-knee*0.5?0.0:
                (over>=knee*0.5?over:(over+knee*0.5)*(over+knee*0.5)/(2.0*knee));
            const double desiredReduction=kneeOver*(2.0/3.0)*std::min(1.0,smoothOpto*20.0);
            const double recovery=0.10+0.95*std::clamp(optoSlow*3.0,0.0,1.0);
            const double gainA=std::exp(-1.0/((desiredReduction>optoReduction?0.008:recovery)*sampleRate));
            optoReduction=gainA*optoReduction+(1.0-gainA)*desiredReduction;
            const double optoGain=1.0+smoothOptoMix*(std::pow(10.0,-optoReduction/20.0)*smoothMakeup-1.0);
            reductionPeak=std::max(reductionPeak,std::clamp(optoReduction*smoothOptoMix/24.0,0.0,1.0));

            for(int ch=0;ch<channels;++ch){
                const double x = in && in[ch] ? static_cast<double>(in[ch][i]) : 0.0;
                inputPeak=std::max(inputPeak,std::abs(x));

                // PREAMP STAGE: OFF / TUBE / TRANSISTOR
                double pre=x*optoGain;
                double preSat=0.0;
                if(preampMode==1){
                    // tube: soft asymmetry, even harmonics, sag and gentle HF rolloff
                    const double absx=std::abs(pre);
                    tubeSag[ch]=tubeSagAtk*tubeSag[ch]+(1.0-tubeSagAtk)*absx;
                    const double sag=1.0/(1.0+tubeSag[ch]*(0.55+1.8*preampDriveNorm));
                    const double z=pre*tubeDrive*sag;
                    const double bias=0.10+0.12*preampDriveNorm;
                    double y=std::tanh(z+bias)-std::tanh(bias);
                    y+=0.10*preampDriveNorm*(y*y)*(y>=0.0?1.0:-0.45);
                    y*=0.78/std::max(0.45,std::sqrt(tubeDrive)*0.22);
                    tubeTone[ch]=(1.0-tubeToneA)*y+tubeToneA*tubeTone[ch];
                    pre=0.72*y+0.28*tubeTone[ch];
                    preSat=std::clamp(std::abs(z-y)/(std::abs(z)+0.25),0.0,1.0);
                }else if(preampMode==2){
                    // transistor: tighter, faster, more odd harmonics and harder knee
                    transistorTone[ch]=(1.0-transistorToneA)*pre+transistorToneA*transistorTone[ch];
                    const double presence=pre-transistorTone[ch];
                    const double z=(pre+0.18*presence)*transistorDrive;
                    const double soft=std::tanh(z*1.15);
                    const double hard=std::clamp(z,-1.15,0.92);
                    pre=(0.48*soft+0.52*hard)/(0.75+0.18*preampDriveNorm);
                    pre+=0.045*preampDriveNorm*pre*pre*pre;
                    preSat=std::clamp(std::abs(z-pre)/(std::abs(z)+0.22),0.0,1.0);
                }

                low[ch]=(1.0-lowA)*pre+lowA*low[ch];
                const double high=pre-low[ch];

                double shapedIn=pre;
                if(shiftMode==1) shapedIn=pre+0.34*high;
                else if(shiftMode==2) shapedIn=pre+0.18*high+0.10*low[ch];

                const double a=std::abs(shapedIn);
                const double coeff=a>env[ch]?atk:rel;
                env[ch]=coeff*env[ch]+(1.0-coeff)*a;
                const double comp=1.0/(1.0+env[ch]*(0.8+2.7*sat));

                const double bias=0.035+0.075*sat;
                const double z=shapedIn*drive*comp;
                const double soft=std::tanh(z+bias)-std::tanh(bias);
                double wet=soft+0.08*sat*std::tanh(z*z*(z>=0?1.0:-1.0));

                const double tapeSat=std::clamp(std::abs(z-soft)/(std::abs(z)+0.35),0.0,1.0)
                                     *std::min(1.0,0.25+1.15*sat);
                saturationPeak=std::max(saturationPeak,std::max(preSat,tapeSat));

                dc[ch]=dcA*dc[ch]+(1.0-dcA)*wet;
                wet-=dc[ch];

                // Always write transport history, including at zero modulation.
                wet=readDelay(ch,wet,std::max(0.0,delaySamples));
                wearLP[ch]=(1.0-wearA)*wet+wearA*wearLP[ch];
                wet=wet*(1.0-0.70*smoothAge)+wearLP[ch]*(0.70*smoothAge);
                if(dropoutLeft[ch]>0){
                    --dropoutLeft[ch];
                }else{
                    dropoutTarget[ch]=0.0;
                    if(smoothAge>0.0001 && uniform()<smoothAge*smoothAge*0.7/sampleRate){
                        dropoutTarget[ch]=(0.15+0.75*uniform())*smoothAge;
                        dropoutLeft[ch]=static_cast<int>((0.012+0.13*uniform())*sampleRate);
                    }
                }
                const double dropA=dropoutTarget[ch]>dropout[ch]?dropAttack:dropRelease;
                dropout[ch]=dropA*dropout[ch]+(1.0-dropA)*dropoutTarget[ch];
                wet*=1.0-dropout[ch];
                const double noise=2.0*uniform()-1.0;
                hissLP[ch]=hissA*hissLP[ch]+(1.0-hissA)*noise;
                const double hiss=smoothAge*smoothAge*0.006*(noise-0.65*hissLP[ch]);
                crackle[ch]*=crackA;
                if(smoothAge>0.0001 && uniform()<(0.2+4.0*smoothAge)*smoothAge/sampleRate)
                    crackle[ch]+=(2.0*uniform()-1.0)*0.045*smoothAge;
                wet+=hiss+crackle[ch]*smoothAge;
                wet*=outGain;
                // The existing additive DRY control stays unprocessed.
                double y=wet+x*dryGain;

                y=runBiquad(y,hp,hp_z1[ch],hp_z2[ch]);
                y=runBiquad(y,lp,lp_z1[ch],lp_z2[ch]);

                outputPeak=std::max(outputPeak,std::abs(y));
                if(out && out[ch]) out[ch][i]=static_cast<Sample>(y);
            }

        }
    }

private:
    struct Biquad { double b0{},b1{},b2{},a1{},a2{}; };

    struct Motion { double from=0.0,to=0.0; int elapsed=0,length=0; };
    double uniform(){
        // Local PRNG: no locks, allocations or global RNG in the audio callback.
        randomState^=randomState<<13; randomState^=randomState>>17; randomState^=randomState<<5;
        return static_cast<double>(randomState)/4294967296.0;
    }
    double nextMotion(Motion& motion,double minSeconds,double maxSeconds){
        if(motion.elapsed>=motion.length){
            motion.from=motion.to;
            motion.to=2.0*uniform()-1.0;
            motion.length=std::max(1,static_cast<int>((minSeconds+(maxSeconds-minSeconds)*uniform())*sampleRate));
            motion.elapsed=0;
        }
        const double t=static_cast<double>(motion.elapsed++)/motion.length;
        return motion.from+(motion.to-motion.from)*t*t*(3.0-2.0*t);
    }
    static double mapLog(double n,double lo,double hi){
        n=std::clamp(n,0.0,1.0);
        return lo*std::pow(hi/lo,n);
    }
    static double mapQ(double n){
        n=std::clamp(n,0.0,1.0);
        return 0.5+11.5*n;
    }

    Biquad makeLowPass(double freq,double q) const { return makeFilter(freq,q,false); }
    Biquad makeHighPass(double freq,double q) const { return makeFilter(freq,q,true); }

    Biquad makeFilter(double freq,double q,bool highPass) const {
        const double pi=3.14159265358979323846;
        freq=std::clamp(freq,10.0,sampleRate*0.45);
        q=std::clamp(q,0.5,12.0);
        const double w0=2.0*pi*freq/sampleRate;
        const double c=std::cos(w0);
        const double s=std::sin(w0);
        const double alpha=s/(2.0*q);
        double b0,b1,b2;
        if(highPass){
            b0=(1.0+c)*0.5; b1=-(1.0+c); b2=(1.0+c)*0.5;
        }else{
            b0=(1.0-c)*0.5; b1=1.0-c; b2=(1.0-c)*0.5;
        }
        const double a0=1.0+alpha;
        Biquad b;
        b.b0=b0/a0; b.b1=b1/a0; b.b2=b2/a0;
        b.a1=(-2.0*c)/a0; b.a2=(1.0-alpha)/a0;
        return b;
    }

    static double runBiquad(double x,const Biquad& b,double& z1,double& z2){
        const double y=b.b0*x+z1;
        z1=b.b1*x-b.a1*y+z2;
        z2=b.b2*x-b.a2*y;
        return y;
    }

    double readDelay(int ch,double input,double delaySamples){
        auto& b=wowBuffer[ch];
        if(b.empty()) return input;
        const size_t size=b.size();
        b[wowWrite[ch]]=input;
        double rp=static_cast<double>(wowWrite[ch])-delaySamples;
        while(rp<0.0) rp+=static_cast<double>(size);
        while(rp>=static_cast<double>(size)) rp-=static_cast<double>(size);
        const size_t i0=static_cast<size_t>(rp);
        const size_t i1=(i0+1)%size;
        const double frac=rp-static_cast<double>(i0);
        const double y=b[i0]*(1.0-frac)+b[i1]*frac;
        wowWrite[ch]=(wowWrite[ch]+1)%size;
        return y;
    }

    double sampleRate=44100.0;
    int numChannels=2;
    double low[2]{},env[2]{},dc[2]{};
    double hp_z1[2]{},hp_z2[2]{},lp_z1[2]{},lp_z2[2]{};
    double wearLP[2]{};
    double tubeSag[2]{},tubeTone[2]{},transistorTone[2]{};
    std::vector<double> wowBuffer[2];
    size_t wowWrite[2]{};
    uint32_t randomState=0x4a65727au;
    Motion wowMotion{},flutterMotion{};
    double smoothWow=0.0,smoothFlutter=0.0,smoothAge=0.0;
    double smoothOpto=0.0,smoothMakeup=1.0,smoothOptoMix=0.0;
    double optoFast=0.0,optoSlow=0.0,optoReduction=0.0;
    double hissLP[2]{},crackle[2]{},dropout[2]{},dropoutTarget[2]{};
    int dropoutLeft[2]{};
};

}
