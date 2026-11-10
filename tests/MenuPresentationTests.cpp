#include "rb/MenuPresentation.hpp"
#include "rb/AfpTimeline.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
int main(int argc,char **argv){try {
    bool rejected=false;try{rb::MenuPresentation menu("",rb::MenuPage(99));}catch(const rb::Error &){rejected=true;}
    if(!rejected)throw rb::Error("invalid menu page accepted");
    if(argc>1){unsigned textures=0,solids=0,draws=0;
        for(auto page:{rb::MenuPage::Title,rb::MenuPage::Music,rb::MenuPage::Result}){
            std::cerr<<"Checking original menu page "<<int(page)<<"\n";
            rb::MenuPresentation menu(argv[1],page);auto frame=menu.frame(0);
            if(frame.sprites.empty()||frame.unresolved.empty())throw rb::Error("menu coverage contract");
            if(menu.frame(120).sprites.size()!=frame.sprites.size())throw rb::Error("static menu changed without recovered scripts");
            for(const auto &d:frame.sprites){++draws;for(auto&p:d.quad)if(!std::isfinite(p.x)||!std::isfinite(p.y))throw rb::Error("nonfinite menu geometry");
                if(d.solidColor){++solids;for(float c:*d.solidColor)if(c<0||c>1)throw rb::Error("invalid original solid color");}
                else{++textures;if(menu.bitmap(d.package,d.texture).bgra.empty())throw rb::Error("empty original menu texture");}
            }
        }
        // Original first title frame is a white untextured rectangle, independently
        // checked against GE2D shape57 flags9 and packed RGBA ffffffff.
        rb::AfpPackage title(rb::readFile((std::filesystem::path(argv[1])/"data/graphics_rb6/title.ifs").string()));
        rb::AfpTimeline tl(title.movieBytes("title"),title.bitStreamBytes("title"));
        auto first=tl.sampleExportedFrame(title,"title",0);
        if(first.size()!=1||!first[0].solidColor||*first[0].solidColor!=std::array<float,4>{{1,1,1,1}})throw rb::Error("original title solid mismatch");
        std::cout<<"PASS original menu resources: "<<draws<<" draws, "<<textures<<" textured, "<<solids<<" solid\n";
    }
    std::cout<<"PASS MenuPresentation invalid page rejection\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
