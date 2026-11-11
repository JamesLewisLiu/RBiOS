#include "rb/OriginalTestMenu.hpp"
#include <iostream>
#include <stdexcept>
using namespace rb;
int main(int argc,char **argv) {try {
    if (argc == 3 && std::string(argv[1]) == "--table") {
        for (const auto &row:originalTestEntries(unsigned(std::stoul(argv[2]))))
            std::cout<<row.scene<<'|'<<row.flags<<'|'<<row.label<<'\n';
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--batch") {
        unsigned region,pressed,repeated,blocked,mode; int32_t selected;
        while(std::cin>>region>>selected>>pressed>>repeated>>blocked>>mode) {
            auto rows=originalTestEntries(region);updateOriginalTestExitFlags(rows,blocked,mode);
            const auto r=updateOriginalTestMenu(rows,selected,pressed,repeated);
            std::cout<<selected<<' '<<int(r.action)<<' '<<r.scene<<' '<<r.requestEventMode<<'\n';
        } return 0;
    }
    auto require=[](bool v){if(!v)throw std::runtime_error("original TEST regression");};
    auto rows=originalTestEntries(0);require(rows.size()==18);require(originalTestEntries(3).size()==16);
    int32_t index=14;updateOriginalTestMenu(rows,index,0,2);require(index==16);
    updateOriginalTestMenu(rows,index,0,1);require(index==14);
    index=0;updateOriginalTestMenu(rows,index,0,1);require(index==17);
    auto result=updateOriginalTestMenu(rows,index,0x10,2);require(result.action==OriginalTestAction::exit&&!result.requestEventMode&&index==17);
    index=16;result=updateOriginalTestMenu(rows,index,0x20,1);require(result.requestEventMode);
    index=0;result=updateOriginalTestMenu(rows,index,0x10,2);require(result.action==OriginalTestAction::enterScene&&result.scene==39&&index==0);
    updateOriginalTestExitFlags(rows,true,1);require(rows[16].flags==2&&rows[17].flags==2);
    index=14;updateOriginalTestMenu(rows,index,0,0x43);require(index==0);
    updateOriginalTestExitFlags(rows,false,1);require(rows[16].flags==0&&rows[17].flags==0);
    updateOriginalTestExitFlags(rows,false,0);require(rows[16].flags==2&&rows[17].flags==0);
    rows[0].flags=1;index=17;updateOriginalTestMenu(rows,index,0,2);require(index==1);
    bool rejected=false;try {originalTestEntries(4);}catch(const std::invalid_argument&){rejected=true;}require(rejected);
    for(auto &r:rows)r.flags=3;rejected=false;try{updateOriginalTestMenu(rows,index,0,2);}catch(const std::invalid_argument&){rejected=true;}require(rejected);
    std::cout<<"PASS original TEST tables, navigation, flags, confirmation and failure boundary\n";
} catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
