# Original stage score labels

The normal battle presentation adds the original `score_you_red/blue` and `score_rival_red/blue` textures from `data/graphics_rb6/gmparts.ifs`. Both sides retain the original 124 x 14 pixels. No fonts, artwork or converted resources are generated.

## Code and resource evidence

Original initialization `0x10152E60` binds AFP objects `score_you` and `score_rival` to the corresponding texture names. The configured local color selects red or blue; the rival receives the other color. Life/course mode uses a different branch and is outside this restoration.

`CULTexture` constructor `0x10071440` clears offset +180. Consequently its draw routine `0x10071070` uses the named AFP texture replacement branch, which calls `0x10029B90`; it does not use the alternative standalone renderer branch. Default extra scale is -1, angle adjustment is zero and visibility starts enabled. The labels inherit the base movie layer **76**, assigned by stage initialization `0x10153E80` to `gmbase_00`. Score digits are a separate layer-75 draw path.

Original `gmparts/gmbase_00` normal placements provide:

| Object | Center | Rotation origin | Matrix | Texture size |
| --- | --- | --- | --- | --- |
| `score_you` | (384,582) | (62,7) | Identity scale/rotation | 124 x 14 |
| `score_rival` | (384,560) | (62,7) | Identity scale/rotation | 124 x 14 |

The original source symbol is 28. The four replacement images all have the same dimensions as this placement. `drawStageLabels` emits the equivalent normal-layout sprite with a centered anchor, unit scale and alpha, layer 76. Its optional positions allow later AFP layout playback to supply updated centers.

The Official Hex-Rays IDA MCP Server freshly decompiled `0x10152E60`, `0x10071070`, `0x10071440` `0x10070FC0` and `0x10153E80` during this pass. The local evidence is `official-ida-label-decompile.json` in the ignored analysis output directory. Saved decompilation and original package metadata provide additional context. The database is an analysis copy; original resources remain untouched.

## Verification and limits

The host component test checks both color assignments, normal positions, anchors and rejection of an invalid color. Given an original `gmparts.ifs` argument, it decodes all four label images in memory and verifies their dimensions.

`tools/verify_stage_labels.py` compares native `--describe` output with the original AFP placement records and texture manifest through the independent upstream parser. It checks both player colors, all four labels, identity transforms, positions, dimensions and original rotation-origin fractions. Layer 76 comes from native initialization evidence rather than the resource parser.

```powershell
python -B G:/vibe/RBiOS/tools/verify_stage_labels.py --game-root E:/MBR-2019121100/IDA --dependencies E:/MBR-2019121100/IDA/RBiOS/.tools --executable E:/MBR-2019121100/IDA/RBiOS/.build/active/rbstagelabelgraphic.exe --report E:/MBR-2019121100/IDA/RBiOS/.build/stage-label-resource-parity.json
```

This is a normal-layout restoration. Animated base-movie transitions, course/life variants, account text and complete original stage scenes remain separate work. Resource and static-code correspondence do not establish whole-VM behavior or original Windows/iPad pixel equivalence.

Observed results: the independent label comparison passed all four labels across both colors. Integrated rendering passed four charts across all difficulties with 738 sampled frames, 46,765 draws and 194 original bitmaps. Each integrated sample requires both score-label draws on layer 76.
