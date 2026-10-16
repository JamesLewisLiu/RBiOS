#include "rb/MusicSheet.hpp"
#include <filesystem>
#include <iostream>
using namespace rb;
namespace {
Bytes hex(const std::string &s) {
    if (s.size() % 2) throw Error("bad hex length");
    Bytes out;
    for (size_t i=0;i<s.size();i+=2) out.push_back(uint8_t(std::stoul(s.substr(i,2),nullptr,16)));
    return out;
}
void print(const MusicSheet &s) {
    auto array=[](const auto &a) { for (auto x:a) std::cout << int64_t(x) << ' '; };
    array(s.counts.total);array(s.counts.justReflec);array(s.counts.extra);array(s.counts.afterJustReflec);array(s.counts.longTicks);array(s.counts.objects);
    std::cout << s.counts.speedAverage << ' ' << s.counts.speedMinimum << ' ' << s.counts.speedMaximum << ' ';
    for(const auto &o:s.objects) {
        array(o.attributes);
        for(auto x:{o.start,o.travel,o.index,o.side,o.oppositeSide,o.duration,o.rawDestination,o.destination,o.widthMode,o.previous,o.next,o.busyTicks,o.routeDestination,o.routeDirection,o.launchLane,o.directionFlags,o.fixedDestination,int32_t(o.wide),int32_t(o.extraIndices.size())}) std::cout<<x<<' ';
        array(o.allowedDestinations);
    }
    std::cout << '\n';
}
}
int main(int argc,char **argv) {
    try {
        if (argc>1 && std::string(argv[1])=="--batch") {
            char op;
            while(std::cin>>op) {
                if(op=='D') {
                    DestinationMaker maker(0);unsigned side;bool top;int32_t duration,time;
                    std::cin>>duration>>time>>side>>top;
                    for(auto &w:maker.random.words)std::cin>>w;
                    for(auto &s:maker.reservations)for(auto &r:s) { for(auto &v:r.begin)std::cin>>v;for(auto &v:r.end)std::cin>>v; }
                    std::array<uint8_t,10> allowed;for(auto &a:allowed){int x;std::cin>>x;a=uint8_t(x);}
                    std::cout<<maker.choose(duration,time,side,top,allowed)<<' ';
                    for(auto w:maker.random.words)std::cout<<w<<' ';
                    for(const auto &s:maker.reservations)for(const auto &r:s) {for(auto v:r.begin)std::cout<<v<<' ';for(auto v:r.end)std::cout<<v<<' ';}
                    std::cout<<'\n';
                } else {
                    std::string bytes;SheetOptions options;options.unequalSidesAllowed=true;
                    std::cin>>options.localChartSide>>options.difficulty>>options.seed>>bytes;
                    auto sheet=MusicSheet::load(Chart::parse(hex(bytes)),options);
                    if(op=='P')sheet.prepare();else if(op!='L')throw Error("bad operation");
                    print(sheet);
                }
                if(!std::cin)throw Error("truncated sheet fixture");
            }
            return 0;
        }
        size_t checks=0;
        auto expect=[&](bool ok){if(!ok)throw Error("music sheet test failed");++checks;};
        SheetOptions options;options.unequalSidesAllowed=true;
        Chart c;c.version=20;
        ChartObject o;o.startMs=-4000;o.travelMs=1000;o.parameters[1]=0;o.flags=0x10;o.group=-1;
        c.objects.push_back(o);o.id=1;o.attributes[1]=1;o.attributes[3]=1;o.parameters[0]=1000;c.objects.push_back(o);
        auto sheet=MusicSheet::load(c,options);
        expect(sheet.objects[0].start==-150 && sheet.objects[0].destination==6);
        expect(sheet.counts.total[0]==1 && sheet.counts.total[1]==6);
        sheet.prepare();expect(sheet.initialObjects.size()==2 && sheet.objects[0].routeDestination==15);
        expect(sheet.objects[0].launchLane==0 && sheet.objects[1].launchLane==6);
        DestinationMaker maker(77);std::array<uint8_t,10> mask{};mask[3]=1;
        expect(maker.choose(7,100,0,false,mask)==3);
        expect(maker.reservations[0][3].end[2]==107 && maker.reservations[0][3].end[1]==137);
        expect(maker.choose(0,100,0,false,mask)==-1);
        if(argc==2) {
            size_t charts=0,objects=0;
            for(const auto &entry:std::filesystem::directory_iterator(std::filesystem::path(argv[1])/"data/sound/ply2"))if(entry.path().extension()==".ply") {
                auto data=Chart::parse(readFile(entry.path().string()));
                auto real=MusicSheet::load(std::move(data),options);real.prepare();
                for(const auto &n:real.objects)expect(n.routeDestination>=8 && n.routeDestination<=27);
                objects+=real.objects.size();++charts;
            }
            std::cout<<"audited "<<charts<<" charts / "<<objects<<" prepared objects\n";
        }
        std::cout<<"MusicSheet: "<<checks<<" checks\n";return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
