#include "rb/AvsDefaultMountOptions.hpp"
#include <array>
#include <string>
namespace rb {
namespace {
#include "AvsMountOptionTables.inc"
bool space(unsigned char c) { return c==' ' || (c>=9 && c<=13); }
// Original 0x10010770 is a custom base-0 integer parser, not libc strtol:
// arithmetic wraps at 32 bits and consumed sign/prefix-only inputs yield zero.
bool booleanValue(std::string_view text, bool &value) {
    if (text.empty()) { value=false; return true; }
    size_t index=0;
    while(index<text.size() && space(text[index])) ++index;
    if(index<text.size() && (text[index]=='+' || text[index]=='-')) ++index;
    unsigned base=10;
    if(index<text.size() && text[index]=='0') {
        base=8;
        if(index+1<text.size() && (text[index+1]=='x' || text[index+1]=='X')) {base=16;index+=2;}
    }
    uint32_t number=0;
    for(;index<text.size();++index) {
        const auto c=text[index];
        const unsigned digit=c>='0'&&c<='9'?unsigned(c-'0'):c>='a'&&c<='f'?unsigned(c-'a'+10):c>='A'&&c<='F'?unsigned(c-'A'+10):99;
        if(digit>=base) break;
        number=number*base+digit;
    }
    value=number!=0;
    return index==text.size();
}
}
int32_t parseAvsDefaultMountOptions(std::string_view text, AvsDefaultMountOptions &output) {
    output={};
    constexpr int32_t invalid=int32_t(0x80070016u);
    std::array<char,272> scratch{};
    size_t used=0,cursor=0,key=0;
    int value=-1,known=0;
    unsigned state=0,shift=12,flags=4;
    auto append=[&](char c){if(used==scratch.size())return false;scratch[used++]=c;return true;};
    while(true) {
        const unsigned char c=cursor<text.size()?static_cast<unsigned char>(text[cursor++]):0;
        unsigned type;
        if(!c) type=7;
        else if(c==',') type=1;
        else if(space(c)) type=2;
        else type=(classifier[c>>1]>>((c&1)*4))&15;
        uint32_t action=transitions[state][type];
        while(action) {
            switch(action&15) {
            case 1: if(!append(char(c)))return invalid; break;
            case 2: key=used; break;
            case 3:
                if(!append(0))return invalid;
                if(std::string_view(scratch.data()+key)=="posix") known=1;
                else if(std::string_view(scratch.data()+key)=="vf") known=2;
                else known=0;
                flags=known?2:4;shift=known?8:12;break;
            case 4: value=int(used);break;
            case 5: if(!append(0))return invalid;break;
            case 6:
                if(((action>>4)&flags)!=0) {
                    if(known) {
                        bool parsed=false;
                        if(value<0 || !booleanValue(std::string_view(scratch.data()+value),parsed))return invalid;
                        if(known==1) output.posix=parsed;else output.volumeFlush=parsed;
                    }
                    used=0;key=0;value=-1;known=0;
                }
                break;
            case 7: {
                const unsigned next=(action>>shift)&15;
                if(next>=9)return next==9?0:invalid;
                state=next;action=0;continue;
            }
            default:return invalid;
            }
            action>>=8;
        }
    }
}
}
