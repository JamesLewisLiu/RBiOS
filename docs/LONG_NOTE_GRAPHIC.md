# Original hold and Switch graphics

LongNoteGraphic.hpp/.cpp returns original texture/AFP draw descriptions. No procedural replacement line or exported asset image is introduced. Evidence is the fixed original DLL, decompilation and raw x86 instructions; pointers are replaced by typed native fields.

## Instruction map

| Address | Restored behavior |
| --- | --- |
| 1015C1A0 | Body/tail/background selection by color, Top and presentationFlags; Top tail layer 39, ordinary 40 |
| 1015C540 | Body layer 62/alignment(3,1), tail 40/(3,3), background 61/(3,3); original blend 4102 maps to AFP8 |
| 1015C2D0 | Endpoints: distance/312, body X scale=stageScale, original float stores, position and angle |
| 1015C400 | Direction from velocity when endpoints coincide; retains existing body scale |
| 1015BF90 | Nonuniform body/tail/background draws and explicitly enabled head/tail effects |
| 101569E0 | Up to 16 Switch segments; separately truncated extra time/duration tick endpoints; special bits 0/1 |
| 10157250 | Cumulative segment geometry; velocity extrapolation beyond secondaryTrailTicks; integer truncation toward zero |
| 10156D40 | clipY clipping, alternating original body/cap movies and final even static tail/background |
| 10156870 / 101568E0 | Body/final-background held on/off labels; intermediate caps always use off |
| 101576F0 | Explicit LongGraphicOverlay effect requests, not substitute pictures |
| 1002B250 | Original alignment 1=0,2=size,3=half-size -> anchor 0/1/.5 |

Original 1017A1AA implements x87 atan2. Hex-Rays loses its floating arguments and shows misleading expressions. Actual instructions load one/zero to obtain float pi/2, then subtract a second float atan2.

1015C2D0 uses float(pi/2)-float(atan2(headY-tailY,tailX-headX)). 1015C400 and 10157250 use float(pi/2)-float(atan2(velocityY,-velocityX)). Decompiled pseudo-constants are not retained.

The original body is 92x312; tails/backgrounds are 72x72. Ordinary body is positioned at the tail endpoint with anchorY=0. Switch bodies use segment start with anchorY=1. They cannot share a guessed endpoint/anchor convention.

## State connection

All coordinates remain original 768x1360. Display scale belongs to ViewportTransform. stageScale is original stage scale, normally one.

| Ordinary hold input | Original / native source |
| --- | --- |
| active | Component+20; not inferred solely from head active |
| head | note+100/+104 -> NoteFrame.x/y |
| tail | note+316/+320 -> endpointX/Y, updated only after holdTrail |
| Direction-only update | Current position and MotionPosition.delta; preserve previous body scale |
| bodyScaleX | stage+24 from endpoint update |
| tailScale | stageScale * note+268 |
| alpha | note+280 |
| headEffect/tailEffect | Real original component instances +112/+116; default disabled |

Each note retains LongNoteGraphicState. Reconstructing defaults each frame would lose the scale deliberately preserved by 1015C400.

| Switch input | Original / native source |
| --- | --- |
| head | graphic+316/+320 <- note+100/+104 |
| trail | graphic+324/+328 <- note+352/+356 |
| velocity | graphic+332/+336 <- note+176/+180 |
| trailTicks / secondaryTrailTicks | graphic+340/+344 <- note+344/+348 |
| scale / alpha | graphic+352/+348 <- stageScale*note+268 / note+280 |
| clipY | graphic+620 from global 12477E34, default 1098 |
| segments.duration | Separate graphic durations from individually truncated extra time/duration; original order, max 16 |
| special / secondarySpecial | ChartExtra.value bits 0/1 |
| collection | 10156950 replay association, attributes[3]>1 excluding 4/5; never inferred from held/special |

The graphic-duration calculation differs from judgment switchDurations, which truncates a combined time. The two arrays remain separate. updateSwitchLongGraphic precedes drawSwitchLongGraphic.

movie distinguishes AFP from bitmap. package identifies the original IFS: usually gmobject, with exp_loop in gmexp_effect. slot identifies a stable per-note movie instance whose age and labels survive through its actual pool state. Special overlay requests need the original queue consumer.

## Validation

Eight host checks pass. tools/verify_long_graphic.py runs original initialization/update/draw instructions:

- 180 ordinary hold cases: colors, Top, sides, flags, endpoint/direction branches, scales and alpha.
- 240 Switch cases:1-16 segments, timers/clipping, active flags, effects, collection and final-tail branches.
- All 420 match texture names, integer positions, float angle/scale/alpha bit patterns, layers, anchors, blends and effect requests.

Renderer virtuals capture parameters; geometry, clipping and branches execute original instructions. Pool/texture allocation is a capture adapter. Labels/alignment/layers are also checked against initialization. Report: .build/long-graphic-parity.json.

tools/verify_long_graphic_assets.py compares ten original visible bitmap BGRA results with an independent reader. Seven required AFP movies,902 frames total, parse/sample: swich_long_body_on/off, long_tail_bg_switch, eff_long_tail_red/blue, eff_justcollection and exp_loop. Report: .build/long-graphic-assets.json. This is decoding coverage, not original-engine pixel parity.

[STAGE_LONG_PRESENTATION](STAGE_LONG_PRESENTATION.md) now connects these components across the full catalog. Replay collection, special queue consumers, full exhaustion semantics and ARM64/device pixels remain unfinished or unverified.
