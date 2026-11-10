#include "rb/StageSession.hpp"
#include "rb/StageScoreInitialization.hpp"
#include "rb/StageContinuousSound.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
using namespace rb;
namespace {
Chart fixture(int kind, bool overlap, int side = 0) {
    Chart chart; chart.version=20; chart.bpm=120; chart.durationMs=10000;
    for (int i=0; i<(overlap ? 2 : 1); ++i) {
        ChartObject object; object.id=int16_t(i); object.group=-1;
        object.startMs=i*500; object.travelMs=1000; object.flags=0x10;
        object.attributes[1]=uint8_t(side); object.attributes[3]=uint8_t(kind);
        object.parameters[0]=1000;
        chart.objects.push_back(object);
        if (kind==4) for (int part=0; part<4; ++part) {
            ChartExtra e; e.object=int16_t(i); e.timeMs=object.startMs+1000+part*250; e.durationMs=250;
            chart.extras.push_back(e);
        }
    }
    if (side==1) {
        ChartObject local; local.id=10; local.group=-1; local.travelMs=1000; local.flags=0x10;
        chart.objects.push_back(local);
    }
    return chart;
}
std::vector<int> run(const Chart &chart, int mode, bool batched, bool manualContact = false) {
    SheetOptions so; so.unequalSidesAllowed=true;
    auto sheet=MusicSheet::load(chart,so); sheet.prepare();
    SessionOptions options; options.inputModes={mode,1};
    options.initialBattle=initializeStageScore(StageScoreInitialization::offline(sheet,1));
    std::vector<int> active;
    options.onPresentationFrame=[&](const SessionSnapshot &s) {
        if (s.tick != int(active.size())-150) throw Error("continuous audio callback tick lost");
        active.push_back(s.holdSoundActive ? 1 : 0);
    };
    StageSession session(std::move(sheet),options);
    if (batched) session.advanceToTick(1000,{});
    else for (int tick=-150; tick<=1000; ++tick) {
        std::vector<TouchPoint> points;
        if (manualContact && tick>=60 && tick<80) {
            for (const auto &note:session.snapshot().notes)
                if (note.side==0) points.push_back({note.x,note.y});
        }
        if (manualContact && tick==80) session.cancelTouches();
        session.advanceToTick(tick,points);
    }
    const auto end=session.snapshot();
    if (!end.complete || end.holdSoundActive || !end.notes.empty()) throw Error("loop retained after release");
    return active;
}
}
int main(int argc, char **argv) {
    try {
        if (argc==3 && std::string(argv[1])=="--resources") {
            const auto path=std::filesystem::path(argv[2])/StageContinuousSound::resource().path;
            const auto wave=Wave::parse(readFile(path.string()));
            uint64_t samples=0; bool audible=false;
            for (size_t i=0;i<wave.blockCount();++i) {
                const auto pcm=wave.decodeBlock(i); samples+=pcm.size();
                for (auto sample:pcm) audible=audible || sample!=0;
            }
            if (!audible || samples!=wave.frames*2 || wave.channels!=2 || wave.sampleRate!=44100)
                throw Error("original loop PCM invalid");
            std::cout << "PASS original loop decoded in memory: " << samples*2 << " bytes\n";
            return 0;
        }
        unsigned cases=0;
        for (int kind : {1,4}) {
            const auto single=run(fixture(kind,false),1,true);
            const auto overlap=run(fixture(kind,true),1,true);
            for (bool pair : {false,true}) for (int mode : {0,1}) {
                const auto chart=fixture(kind,pair);
                const auto batched=run(chart,mode,true), stepped=run(chart,mode,false);
                if (batched!=stepped) throw Error("loop state depends on batching");
                const auto count=std::count(batched.begin(),batched.end(),1);
                if (mode==0 && count!=0) throw Error("manual no-contact started loop");
                if (mode==1 && count==0) throw Error("auto long note never starts loop");
                ++cases;
            }
            if (std::count(overlap.begin(),overlap.end(),1)<=std::count(single.begin(),single.end(),1))
                throw Error("overlapping hold did not extend active interval");
            for (size_t i=0;i<single.size();++i)
                if (single[i] && !overlap[i]) throw Error("overlap interrupted existing loop");
            const auto manual=run(fixture(kind,false),0,false,true);
            if (std::count(manual.begin(),manual.end(),1)==0)
                throw Error("manual contact never starts long sound");
            if (manual[150+200]) throw Error("manual loop retained after contact cancellation and note expiry");
            const auto cpu=run(fixture(kind,false,1),1,true);
            if (std::count(cpu.begin(),cpu.end(),1)) throw Error("rival held note started local loop");
        }
        std::cout << "PASS " << cases << " batched/per-tick Hold/Switch sessions, overlap, manual contact/cancellation and rival silence\n";
        return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
