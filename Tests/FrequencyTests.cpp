#include "SwellEngine.h"
#include <complex>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
constexpr double pi = 3.14159265358979323846;
void check(bool pass, const char* message) { if (!pass) throw std::runtime_error(message); }
double magnitude(const std::vector<double>& x, double hz, double sr)
{
    std::complex<double> sum {};
    for (size_t i=0; i<x.size(); ++i) sum += x[i] * std::polar(1.0, -2*pi*hz*static_cast<double>(i)/sr);
    return 2.0 * std::abs(sum) / static_cast<double>(x.size());
}
void spectralTests()
{
    double worstImageDb = -200;
    for (double sr : {22050.0, 44100.0, 48000.0, 96000.0, 192000.0})
        for (double tone : {40.0, 1000.0, 6000.0})
            for (double shift : {-200.0, 200.0})
            {
                bloom::FrequencyShifter shifter; shifter.prepare(sr); shifter.setEndpoints(shift, shift, true);
                std::vector<double> audio(static_cast<size_t>(sr));
                for (int i=0; i<static_cast<int>(sr*2); ++i)
                {
                    shifter.beginSample(shift);
                    double y=shifter.process(0.1*std::cos(2*pi*tone*i/sr),0);
                    if (i >= static_cast<int>(sr)) audio[static_cast<size_t>(i-static_cast<int>(sr))]=y;
                }
                const double desired=magnitude(audio,std::abs(tone+shift),sr);
                const double image=magnitude(audio,std::abs(tone-shift),sr);
                check(desired>0.09 && desired<0.101, "Shifted tone must have the correct frequency and level");
                check(image < desired*0.001, "Unwanted sideband must be at least 60 dB below the wanted shift");
                worstImageDb=std::max(worstImageDb,20*std::log10(std::max(image/desired,1e-15)));
            }
    std::cout << "Worst tested unwanted sideband: " << worstImageDb << " dB\n";
    // High-frequency input shifted above Nyquist is attenuated before modulation.
    for (double sr : {44100.0,48000.0,96000.0})
    {
        bloom::FrequencyShifter shifter; shifter.prepare(sr); shifter.setEndpoints(5000,5000,true);
        double energy=0;
        for (int i=0; i<static_cast<int>(sr); ++i)
        {
            shifter.beginSample(5000);
            double y=shifter.process(0.1*std::cos(2*pi*(sr*0.5-1000)*i/sr),0);
            if (i>static_cast<int>(sr/2)) energy+=y*y;
        }
        check(std::sqrt(energy/(sr/2))<0.0001,"Anti-alias guard suppresses tones that would wrap above Nyquist");
    }
}
void sweepTests()
{
    for (bool outward : {false,true})
    {
        bloom::Parameters p; p.mode=bloom::Mode::frequencyShift; p.shiftBottomHz=-500; p.shiftTopHz=1000;
        p.attackMs=100; p.outward=outward;
        bloom::SwellEngine engine; engine.setParameters(p); engine.prepare(48000);
        double previous=outward?1000:-500;
        for(int i=0;i<9600;++i)
        {
            float sample=0.2f; float* ptr=&sample; engine.process(&ptr,1,1);
            const auto shift=engine.getShiftFrequency();
            if(i==0) check(std::abs(shift-previous)<0.001,"Sweep starts at the selected endpoint");
            check(outward?shift<=previous+0.001:shift>=previous-0.001,"Frequency sweep stays one-way");
            previous=shift;
        }
        check(std::abs(previous-(outward?-500:1000))<0.001,"Frequency sweep reaches and holds the selected endpoint");
    }
}
void mixAndStereoTests()
{
    for(auto mode : {bloom::Mode::volume,bloom::Mode::highPass,bloom::Mode::lowPass,bloom::Mode::frequencyShift})
    {
        bloom::Parameters p; p.mode=mode; p.shiftBottomHz=-200; p.shiftTopHz=300;
        bloom::SwellEngine wet,dry,half;
        wet.setParameters(p); wet.prepare(48000);
        p.mixPercent=0; dry.setParameters(p); dry.prepare(48000);
        p.mixPercent=50; half.setParameters(p); half.prepare(48000);
        std::vector<float> input(48000);
        for(size_t i=0;i<input.size();++i) input[i]=static_cast<float>(0.1*std::sin(2*pi*1000*static_cast<double>(i)/48000));
        auto w=input,d=input,h=input,r=input;
        for(auto& x:r)x*=-2;
        float* wp[]={w.data(),r.data()}; float* dp=d.data(); float* hp=h.data();
        wet.process(wp,2,48000); dry.process(&dp,1,48000); half.process(&hp,1,48000);
        check(std::memcmp(d.data(),input.data(),d.size()*sizeof(float))==0,"0% Mix must pass undelayed dry audio at unity trims");
        // The onset of a sinusoid occurs a few samples earlier with a louder stereo
        // partner, so test the settled blend after all sweeps reach their endpoint.
        for(size_t i=40000;i<input.size();++i)
        {
            // Frequency shift needs matching envelope/oscillator history for the mix check below.
            if(mode!=bloom::Mode::frequencyShift)
                check(std::abs(h[i]-0.5f*(input[i]+w[i]))<1e-5f,"Master Mix is a linear dry/wet crossfade");
            check(std::abs(r[i]+2*w[i])<1e-5f,"Frequency shifting preserves stereo channel relationships");
        }
        // Mono wet and half-wet engines share identical trigger history.
        bloom::SwellEngine monoWet; p.mixPercent=100; monoWet.setParameters(p); monoWet.prepare(48000);
        auto mono=input; auto* mp=mono.data(); monoWet.process(&mp,1,48000);
        for(size_t i=0;i<input.size();++i)
            check(std::abs(h[i]-0.5f*(input[i]+mono[i]))<1e-6f,"Master Mix combines dry and processed paths correctly in every mode");
    }
    bloom::Parameters p; p.mode=bloom::Mode::bypass;p.mixPercent=37;p.inputDb=24;p.outputDb=-20;
    bloom::SwellEngine bypass;bypass.setParameters(p);bypass.prepare(48000);
    float sample=.1234f,copy=sample; auto* ptr=&sample;bypass.process(&ptr,1,1);
    check(std::memcmp(&sample,&copy,sizeof(float))==0,"Bypass ignores Master Mix and trims");
}
void stressTests()
{
    for(double sr : {11025.0,44100.0,192000.0})
    {
        bloom::Parameters p;p.mode=bloom::Mode::frequencyShift;p.attackMs=1;
        bloom::SwellEngine engine;engine.setParameters(p);engine.prepare(sr);
        for(int block=0;block<1500;++block)
        {
            p.shiftTopHz=block%2==0?5000:-5000;p.shiftBottomHz=-p.shiftTopHz;p.outward=block%3==0;
            p.mixPercent=static_cast<float>(block%101);engine.setParameters(p);
            std::array<float,127> data;
            for(int i=0;i<127;++i)data[static_cast<size_t>(i)]=static_cast<float>(0.1*std::sin((block*127+i)*0.13));
            auto* ptr=data.data();engine.process(&ptr,1,127);
            for(float x:data)check(std::isfinite(x)&&std::abs(x)<2,"Extreme frequency automation remains finite and bounded");
        }
    }
}
}
int main()
{
    try {spectralTests();sweepTests();mixAndStereoTests();stressTests();std::cout<<"PASS: frequency shifting, endpoints, stereo, mix, alias guard, automation.\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
