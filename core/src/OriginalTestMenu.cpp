#include "rb/OriginalTestMenu.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace rb {
std::vector<OriginalTestEntry> originalTestEntries(unsigned region) {
    if (region > 3) throw std::invalid_argument("original TEST region outside table");
    std::vector<OriginalTestEntry> result{{39,"I/O CHECK"},{44,"SCREEN CHECK"},
        {45,"COLOR CHECK"},{46,"ROM CHECK"},{47,"IC CARD CHECK"},{63,"SOUND OPTIONS"},
        {64,"GAME OPTIONS"},{53,"COIN OPTIONS"}};
    if (region == 0) result.push_back({54,"ECOMODE OPTIONS"});
    for (auto entry : {OriginalTestEntry{48,"NETWORK OPTIONS"},{51,"BOOKKEEPING"},{52,"CLOCK"}})
        result.push_back(entry);
    if (region == 0) result.push_back({55,"VIRTUAL COIN"});
    for (auto entry : {OriginalTestEntry{66,"ALL FACTORY SETTINGS"},{65,"SYSTEM INFORMATION"},
        {std::numeric_limits<int32_t>::min(),""},{-4,"EVENT MODE"},{-5,"GAME MODE"}})
        result.push_back(entry);
    return result;
}
void updateOriginalTestExitFlags(std::vector<OriginalTestEntry> &entries, bool blocked, uint16_t mode) {
    auto game = std::find_if(entries.begin(),entries.end(),[](const auto &e){return e.scene == -5;});
    auto event = std::find_if(entries.begin(),entries.end(),[](const auto &e){return e.scene == -4;});
    if (game == entries.end() || event == entries.end()) throw std::invalid_argument("TEST exit entries absent");
    game->flags = blocked ? game->flags | 2u : game->flags & ~2u;
    event->flags = !blocked && mode == 1 ? event->flags & ~2u : event->flags | 2u;
}
OriginalTestDecision updateOriginalTestMenu(const std::vector<OriginalTestEntry> &entries,
    int32_t &selected, uint32_t pressed, uint32_t repeated) {
    if (selected < 0 || size_t(selected) >= entries.size()) throw std::invalid_argument("invalid TEST selection");
    if (pressed & 0x30) {
        const auto scene = entries[size_t(selected)].scene;
        if (scene == -5 || scene == -4) return {OriginalTestAction::exit,0,scene == -4};
        return {OriginalTestAction::enterScene,scene,false};
    }
    const int32_t direction = repeated & 0x42 ? 1 : repeated & 1 ? -1 : 0;
    if (!direction) return {};
    // Original valid tables have selectable rows. Bound malformed inputs instead
    // of reproducing the original infinite scan; never silently choose a row.
    for (size_t i = 0; i < entries.size(); ++i) {
        selected += direction;
        if (selected >= int32_t(entries.size())) selected = 0;
        else if (selected < 0) selected = int32_t(entries.size()) - 1;
        const auto &entry = entries[size_t(selected)];
        if (entry.scene != std::numeric_limits<int32_t>::min() && !(entry.flags & 3)) return {};
    }
    throw std::invalid_argument("TEST table has no selectable row");
}
} // namespace rb
