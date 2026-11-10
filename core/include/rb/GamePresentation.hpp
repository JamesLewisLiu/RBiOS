#pragma once
#include "AFP.hpp"
#include "StageSession.hpp"
#include <array>
#include <memory>
#include <optional>

namespace rb {
struct PresentationSprite {
    std::string identity, package, texture;
    std::array<MotionVector,4> quad; // bottom-left/right, then top-left/right; original Y down
    std::array<float,4> multiply{{1,1,1,1}};
    float alpha=1;
    int32_t layer=40;
    uint32_t blend=0;
    std::vector<std::array<MotionVector, 4>> clips;
    std::optional<std::array<float, 4>> solidColor;
};
struct PresentationFrame {
    std::vector<PresentationSprite> sprites;
    std::vector<std::string> unresolved;
};
// Native platform presentation adapter using restored original draw commands.
// It loads original packages lazily and retains decoded assets only in memory.
class GamePresentation {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    GamePresentation(std::string gameRoot,unsigned difficulty);
    ~GamePresentation();
    void noteCreated(int32_t,const std::vector<SessionMovieBinding>&);
    void noteReleased(int32_t);
    void advance(const SessionSnapshot&); // once per original logic tick
    PresentationFrame frame(const SessionSnapshot&);
    std::vector<std::pair<std::string, std::string>> prepareStageBitmaps();
    const AfpBitmap &bitmap(const std::string &package,const std::string &texture);
};
// Original sprite alignment and non-uniform scale, shared by Apple and host tests.
std::array<MotionVector,4> originalSpriteQuad(float width,float height,float x,float y,
    float angle,float scaleX,float scaleY,float anchorX,float anchorY);
} // namespace rb
