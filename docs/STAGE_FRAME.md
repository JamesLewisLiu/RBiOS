# Original stage frame

The stage frame is loaded from the original `data/graphics_rb6/gmframe_00.ifs`. Its AFP geometry, texture regions, transforms, animation frames and clipping rectangle are interpreted at runtime. The package is not rewritten, and no converted assets are required.

## Original evidence

The reference binary is `modules/reflecbeat.dll`, SHA-256 `ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8`. Addresses below are original image virtual addresses. The saved decompilation is `modules - Copy/reflecbeat.dll.c`; ambiguous graphics calls were checked against PE tables and instructions.

`CStageFrame` initialization at `0x1014C440` selects a frame package from IDs 92 through 97, loads the `gmframe` export on layer 22, and positions it at **(-15, 0)**. The movie is **798 x 1360**, while the game field is **768 x 1360**. The original placement therefore extends 15 pixels beyond each horizontal field boundary. The movie must retain its top-left origin; centering its origin at the middle of the field produces incorrect placement.

The original package exports `gmframe` as symbol 245 and `frame_down` as symbol 238. The main symbol has 38 frames and labels `normal = 1`, `off = 2`, `on = 4`, using the file's one-based label values. Its initial display list contains these elements:

| Element | Source symbol | Placement | Origin | Resulting geometry |
| --- | ---: | --- | --- | --- |
| `frame_round` | 13 | (83, 41) | (55, 20) | 110 x 40 |
| `frame_dif` | 18 | (714, 41) | (53, 20) | 106 x 40 |
| `frame_up` | 21 | (399, 87) | (399, 87) | (0, 0), 798 x 174 |
| Bottom panel | 238 | (399, 1244) | (399, 116) | (0, 1128), 798 x 232 |

A fully transparent 1 x 1 white shape also exists and does not contribute a visible draw.

## Scripts, clipping and clocks

The parsed movie contains two action scripts and one placement trigger. The action methods are `aeplib.stop` (constant `0x440`) and `aeplib.gotoAndPlay` (`0x442`); the latter receives the `normal` label. The normal root timeline stops at zero-based frame 3.

The placement trigger on the main symbol's bottom-panel child invokes `aeplib.aep_set_rect_mask` (`0x833`). Reversing the original stack argument order gives the local rectangle **left = 0, right = 798, top = 0, bottom = 232**. Its PUSH list is `[232, 0, 798, 0, THIS, 5, aeplib]`. AP2 PUSH type 0 denotes integer zero; interpreting it as null prevents this initializer from being recovered correctly.

The bottom-panel symbol and its nested timer symbol 214 each contain 112 frames. They have no stop scripts of their own. They continue to advance while the root remains stopped. Sampling the entire hierarchy at the fixed root frame freezes this animation incorrectly. The sampler therefore tracks elapsed child age separately from the root's selected frame. Native frame start/reset at `0x1014C370` calls `0x10029800(..., 1, 1)` and `0x100297B0(..., 1)`; the inspected code does not establish an additional child seek.

Clip quadrilaterals are propagated in sampler-root coordinates. The scene applies the original outer (-15, 0) translation to both frame graphics and masks. The mask belongs to the bottom-panel branch, not to the complete movie.

## Named texture bindings

The original `CULTexture` binding stores an AFP instance, object name and replacement texture (`0x10070FA0`, `0x10070FC0`, `0x10070F30`). Rendering at `0x10071070` hides the named source, queries its AFP rectangle, alpha and scale, and draws the replacement at the rectangle center. The constructor's extra-scale field is -1. These stage bindings have identical source and replacement dimensions, so replacing the original rectangular texture draw preserves the measured geometry.

| Named object | Original table | Current binding | Original dimensions |
| --- | --- | --- | --- |
| `frame_round` | `0x1036BCEC`: `frame_round_00` through `06` | `frame_round_00` | 110 x 40 |
| `frame_dif` | `0x1036BD08`: `frame_dif_b/m/h/wh` | Selected chart difficulty | 106 x 40 |
| `player_you` | `0x10374460`: `player_you_red/blue` | `player_you_red` | 210 x 18 |
| `player_rival` | `0x10374468`: `player_rival_red/blue` | `player_rival_blue` | 210 x 18 |
| `orb_you`, `orb_rival` | `0x10374470`: `orb_red/blue` | Red player, blue rival | 20 x 20 |

Every replacement above is in the same original package. These names select textures; they are not timeline labels. The first-round binding is a current single-stage selection and does not establish a recovered multi-round session flow.

The original bottom panel also contains account-bound placeholders named `name_you/rival`, `torina_you/rival`, `shop_you/rival`, `area_you/rival`, and `characard_s_you/rival`. Their geometry is untextured colored dummy content: 160 x 16 text areas, 12 x 12 title markers, and 140 x 90 portraits. These named branches are hidden until their real bindings are implemented. Displaying the placeholders would introduce colored boxes that the original application replaces. No account identity or portrait is fabricated.

## Independent comparison

`tools/verify_stage_frame.py` builds a separate reference display list from the upstream bemaniutils AP2 and GE2D parsers at commit `1e66fcd780afb3088ce8e78c88615a143db492ee`. It reads the original archive and geometry in memory. Analysis dependencies are supplied explicitly and are not runtime app dependencies.

The comparison covers all four difficulty bindings at elapsed ticks 0 through 223, then 1000, 10000 and 100000: **908 samples**, including two full 112-frame child cycles. It checks draw order, texture selection, affine matrices, geometry bounds, colors, blend modes, named instance paths, and every inherited mask corner. It also requires all 56 original timer textures to occur. Replacement dimensions and the original action/trigger inventory are checked independently before invoking the native sampler.

Example from PowerShell, with all analysis outputs inside the original analysis directory:

```powershell
python -B G:\vibe\RBiOS\tools\verify_stage_frame.py `
  --game-root E:\MBR-2019121100\IDA `
  --dependencies E:\MBR-2019121100\IDA\RBiOS\.tools `
  --executable E:\MBR-2019121100\IDA\RBiOS\.build\active\rbafptimeline.exe `
  --output E:\MBR-2019121100\IDA\RBiOS\.build\stage-frame-parity.json
```

The native test executable must include its `--frame-sample` mode. The recorded comparison passed **908 samples, 9,080 draws, 6,356 masked draws and all 56 timer textures** using the executable in `.build/active/rbafptimeline.exe`. The report records package SHA-256 `1c5b13c59b8bebffafbd60c15c18654fc2553aa52f6029598470a13c30a6583f` and movie SHA-256 `b5b014aad3f1065de7a57b0716ebbd790406c057e18c521cade34f408547c404`.

Related regression checks also passed: four previously supported AFP packages with 2,040 fixed-frame and 798 label samples; four native charts across every difficulty; and every tick of the `6k60` lastboss W chart, covering 7,498 frames, 635,983 draws and 195 bitmaps. These checks supplement the stage-frame comparison without extending its visual or runtime claims.

The implementation at `9cbc657` also passed [Actions run 42052166298](https://github.com/JamesLewisLiu/RBiOS/actions/runs/42052166298): arm64 device archive, minimum OS 14.0 and all 21 Apple simulator tests. The GPU regression exercises the production renderer with nested rectangular masks at 50%, 75% and 100%, checking both retained and excluded pixels. This uses synthetic markers; it does not compare the original stage scene against Windows pixels.

## Limits

This is a static resource and instruction based restoration of the stage-frame branch. The independent comparison is not an original Windows render trace, a general implementation of every `aeplib` method, or an iPad pixel comparison. Account text, portraits, remaining score widgets, other pages and unimplemented game-session behavior require their own bindings and evidence. Original frame artwork remains proportionally fitted into the fixed logical viewport; iPad aspect-ratio margins do not imply missing source pixels. Real-device rendering, clipping, animation and touch alignment still require installation testing.
