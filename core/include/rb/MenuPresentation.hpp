#pragma once
#include "GamePresentation.hpp"
namespace rb {
enum class MenuPage { Title, Music, Result };
// Original menu art, with unresolved dynamic bindings explicitly reported.
class MenuPresentation {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    MenuPresentation(std::string root, MenuPage page);
    ~MenuPresentation();
    PresentationFrame frame(uint32_t tick);
    const AfpBitmap &bitmap(const std::string &package, const std::string &texture);
};
}
