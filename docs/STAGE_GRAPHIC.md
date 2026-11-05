# Original stage graphics mapping

`drawStageJudgmentGraphics(StageGraphicOptions)` returns existing `SpriteDraw` values referencing unmodified `gmparts.ifs` bitmap names. It applies the original fixed geometry through the shared `FieldGeometry`. No resource file is converted.

## Original evidence

- DLL SHA-256: `ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8`.
- `modules - Copy/reflecbeat.dll.c` 0x1014F640 creates normal judgment lines via tables 0x1036BD24/0x1036BD2C. It centers each sprite `(3,3)` and places both at X=384, Y=`100A4B90(4 or 5)`. Default field initialization 0x100A4C90 yields local Y=1098 / opponent Y=204. No rotation or scale override is applied. Body layer 69/blend 0; background layer 70/blend argument 1 → AFP8/additive.
- Colors use original profile +20: local 0=red / 1=blue; opponent chooses the opposite color. 0x1014F550 supplies separate body/background opacity percentages. 0x1014F400 modulates the latter by original phase; the function's `lineBackgroundAlpha` argument lets the caller supply that state.
- 0x1014F8A0 selects top chip X=128 / 640 / 384 and Y=871 local / 431 opponent, including original geometry offsets. 0x10150260 enables left/right lanes by default, adds the central pair for original difficulty >=2 or profile+1072 equal to 2 or 3, and can suppress opponent chips through profile+1044 equal to 1 or component flag+178. The API exposes lane count and opponent visibility rather than assuming all six always appear.
- 0x1014FEC0 creates `line_top_parts` at layer 67/additive, `line_top_{color}_mark_you/rival` at 69/additive (tables 0x1036BD34), and `line_top_{color}` at 70/normal (0x1036BD44). All are centered. 0x1014FC10 moves all three to the chip position; body/parts opacity equals input percentage /100, marker equals opacity × modulation ×0.5. No per-chip rotation is set.

Defaults represent fully visible lines and two top lanes. Phase-dependent pulse and component fade timing remain caller inputs; this API does not claim to reproduce their state machine. The separate `line_top_eff` / `top_assist` movies and hit feedback are not replaced with static images.

`StageGraphicTests` passes 12 geometry/selection/option checks without assets, and 56 checks with original `gmparts.ifs` (all selected bitmap references decode). These checks are not a trace of virtual graphic calls in the original DLL and do not establish full scene equivalence.

## Background and frame movies

0x10152210 loads one of original package IDs 98/99/100 (`gmbg_00/01/02.ifs`) according to profile selection, using movie names `gmbg_00` and `gmbg_01`, both layer 79. It centers them with alignment(3,3). 0x10151EF0 positions both at(384,680), and initially seeks/plays only movie 0. 0x10152190 switches state when local PlayerScore.finalRate() (0x101643B0) >=0.69999999; 0x10151E80 stops movie 0 and seeks/plays movie 1. A bridge can therefore initially sample original `gmbg_00` frame `elapsed % 240`; switching to movie 1 requires the original achievement-rate state transition and a separate age. See AFP_TIMELINE.md for all-six-movie matrix/texture parity results.

The frame is itself an original `gmframe` AFP movie, layer 22, positioned(-15,0) by 0x1014C440 initialization (decompile around line 277520). The stage now samples its original panels, round/difficulty/player/orb texture bindings, bottom-panel rectangular mask and independently advancing child timelines. AP2 PUSH type 0 is integer zero. Account text and portrait placeholders remain hidden until their real bindings are recovered. See [STAGE_FRAME](STAGE_FRAME.md) for original addresses, 908 independent samples and remaining limits.

AFP layer numbers place backgrounds 79 behind notes 40 and frame 22; the SpriteKit bridge can map layer to a negative Z to preserve this direction.
