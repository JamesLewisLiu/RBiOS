# Original menu artwork and remaining bindings

The native menu adapter samples original AFP/GE2D display lists and unchanged IFS textures. It does not reproduce the complete original menu scripts. `MenuPresentation` deliberately freezes supported frames; its unresolved list identifies missing dynamic bindings rather than silently executing unknown bytecode.

The original title constructor at `0x100C5EF0` allocates title/start/start-text movies from resource group 49 at layers 21/19/18. Fresh Official IDA MCP pseudocode confirms these names. The title AFP is 768x1360, with `skip` at frame 658 and `loop` at 659 (one-based labels). The offline adapter shows zero-based 658, binds the original start movie at its original named-object anchor 385,1067, and omits cabinet payment/card indicators. This is an explicit offline entry policy; attract timing and payment behavior remain unported.

Selection uses `music.ifs` root frame 0 and `main` at the original named anchor 384,530. Results use `result.ifs` root frame 0, `music_main` at 384,247, `result_score_00` at 198,717 and 570,717, and `result_score_01` at 198,1145. Those coordinates come from original display-list placements. Dynamic card lists, jackets, profile data, numeric fields and page animation scripts remain separate recovery work. Static result art does not itself bind the current score.

The original movies refer to textures in matching `_ja.ifs` packages. The archive adapter resolves missing texture names against an explicitly owned, single-level companion package; archive bytes are unchanged. These original localized graphics retain their original language.

## Geometry support

The AP2 sampler now supports GE2D rectangular primitives with flags 9 (instantiable, solid RGBA) and 67 (instantiable texture, normalized region coordinates). It preserves packed RGBA and validates the original six-index rectangle topology. Flag 67 accepts exactly the full region corner UVs; arbitrary UV subregions remain rejected. Unknown geometry and scripts still throw. The existing textured flags 3 path is retained.

The title's first frame independently provides a white rectangle: original shape 57, flags 9, RGBA `ffffffff`. Music shape 97 and result shape 27 use RGBA `c30000ff`; these are preserved primitives, not replacement image assets. Texture/solid colors are multiplied by original display-list color and alpha in the platform renderer. Native solid primitives require no generated resource files.

`MenuPresentationTests` checks invalid page handling, explicit unresolved coverage, finite quad geometry, runtime texture decoding, static snapshot behavior, and the original title solid color when supplied the original resource root. Device scene parity, all menu interactions, original fonts, dynamic movie substitution and the full AFP VM remain unverified.

## Validation for this increment

The rebuilt native original-resource audit passed all three menu pages: 195 draws, 156 textured draws and 39 solid primitives. Every referenced bitmap decoded from the original packages in memory. The existing four-difficulty stage audit remained unchanged at 738 samples, 46,765 draws and 194 used bitmaps. These are resource/geometry checks; the app supplies native controls over static original artwork. They do not establish full original menu behavior or original scene pixel parity.
