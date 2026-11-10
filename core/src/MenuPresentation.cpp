#include "rb/MenuPresentation.hpp"
#include "rb/AfpTimeline.hpp"
#include <filesystem>
#include <map>
namespace rb {
struct MenuPresentation::Impl {
    std::filesystem::path root;
    MenuPage page;
    std::map<std::string, std::unique_ptr<AfpPackage>> packages;
    std::map<std::pair<std::string,std::string>, AfpBitmap> bitmaps;
    PresentationFrame cached;
    Impl(std::string path, MenuPage p):root(std::move(path)),page(p) {}
    AfpPackage &package(const std::string &name) {
        auto &p=packages[name];
        if(!p) p=std::make_unique<AfpPackage>(readFile((root/"data/graphics_rb6"/(name+".ifs")).string()));
        return *p;
    }
    void movie(const std::string &pack,const std::string &name,uint32_t tick,float x,float y,int layer,
               const std::set<std::string> &hidden={}) {
        auto &p=package(pack);
        AfpTimeline timeline(p.movieBytes(name),p.bitStreamBytes(name));
        AfpSampleOptions options;options.hiddenObjects=hidden;
        auto draws=timeline.sampleExportedFrame(p,name,tick,options);
        unsigned part=0;
        const float ox=timeline.location[0]+float(timeline.location[1]-timeline.location[0])/2;
        const float oy=timeline.location[2]+float(timeline.location[3]-timeline.location[2])/2;
        for(const auto &d:draws) {
            std::array<MotionVector,4> q{{{d.bounds[0],d.bounds[3]},{d.bounds[1],d.bounds[3]},
                {d.bounds[0],d.bounds[2]},{d.bounds[1],d.bounds[2]}}};
            for(auto &v:q) {auto xx=v.x,yy=v.y;v={x+d.matrix.a*xx+d.matrix.c*yy+d.matrix.tx-ox,
                y+d.matrix.b*xx+d.matrix.d*yy+d.matrix.ty-oy};}
            if(d.blend!=0 && d.blend!=8) throw Error("unsupported menu blend");
            cached.sprites.push_back({"menu:"+name+":"+std::to_string(layer)+":"+std::to_string(part++),pack,d.texture,q,d.multiply,1,layer,d.blend,{},d.solidColor});
            for(const auto &clip:d.clips){std::array<MotionVector,4> c;for(unsigned i=0;i<4;++i)c[i]={x+clip[i][0]-ox,y+clip[i][1]-oy};cached.sprites.back().clips.push_back(c);}
        }
    }
};
MenuPresentation::MenuPresentation(std::string root,MenuPage page):impl(std::make_unique<Impl>(std::move(root),page)) {
    if(page!=MenuPage::Title && page!=MenuPage::Music && page!=MenuPage::Result) throw Error("invalid menu page");
    const std::string pack=page==MenuPage::Title?"title":page==MenuPage::Music?"music":"result";
    impl->package(pack).addTextureFallback(impl->package(pack+"_ja"));
    // Original display-list frames are frozen until page scripts are recovered.
    // The title starts at the original skip/loop boundary, not the attract intro.
    if(page==MenuPage::Title) {
        impl->movie("title","title",658,384,680,21,{"paseli","icon_00","icon_01","icon_02","epass_info","start"});
        impl->movie("title","start",0,385,1067,19);
        impl->cached.unresolved={"Original attract sequence, cabinet payment and card bindings"};
    } else {
        impl->movie("bg_00","bg",60,384,680,90);
        if(page==MenuPage::Music) {
            impl->movie("music","music",0,384,680,21,{"main","friend_card"});
            impl->movie("music","main",0,384,530,19);
            impl->cached.unresolved={"Original scrolling cards, jackets, font and profile bindings", "Original menu animation scripts"};
        } else {
            impl->movie("result","result",0,384,680,21,{"music","score_you_00","score_you_01","score_rival","stamp","stamp_huki_you","stamp_huki_rival","playshare"});
            impl->movie("result","music_main",0,384,247,19);
            impl->movie("result","result_score_00",0,198,717,18);
            impl->movie("result","result_score_00",0,570,717,17);
            impl->movie("result","result_score_01",0,198,1145,16);
            impl->cached.unresolved={"Original result digit, jacket, profile and stamp bindings", "Original result animation scripts"};
        }
    }
}
MenuPresentation::~MenuPresentation()=default;
PresentationFrame MenuPresentation::frame(uint32_t tick) {(void)tick;return impl->cached;}
const AfpBitmap &MenuPresentation::bitmap(const std::string &pack,const std::string &texture) {
    auto key=std::make_pair(pack,texture);auto found=impl->bitmaps.find(key);
    if(found==impl->bitmaps.end())found=impl->bitmaps.emplace(key,impl->package(pack).bitmap(texture)).first;
    return found->second;
}
}
