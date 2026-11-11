#include "rb/OriginalEntryIdentity.hpp"
#include <algorithm>
namespace rb {
bool OriginalEntryIdentity::apply(const std::array<uint8_t,16> &id) {
    bytes[0]='M'; bytes[1]='B'; bytes[2]='R'; bytes[3]=0;
    if (id[3]) { bytes[4]=id[3]; bytes[5]=0; }
    // Original compares one-character strings with lengths 3 and 10: neither
    // branch can run. Do not repair these into spec/revision assignments.
    if (std::all_of(id.begin()+6,id.end(),[](uint8_t c){return c!=0;})) {
        std::copy(id.begin()+6,id.end(),bytes.begin()+10); bytes[20]=0;
    }
    return id[3]=='K';
}
}
