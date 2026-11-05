# AFP archive binding

AfpPackage reads original named bitmaps, movies, BSI and geometry from IFS. Archive loading alone is not evidence that movie instructions have executed.

## Original evidence

Source: modules - Copy/reflecbeat.dll.c and the fixed game DLL recorded in [MIGRATION](MIGRATION.md).

| Address / decompilation line | Observed call chain | Boundary |
| --- | --- | --- |
| 0x1002AF60 / about 63372 | afpu_get_image_info_at_package -> afpu_get_afp_bitmap_info -> afp_sprite_layer_create_from_info | Named images can be bound without executing a movie |
| 0x1002AFE0 / about 63393 | afp_layer_change_sprite | Changes the image of an existing layer |
| 0x1002B100 / about 63480 | Integer coordinates converted to float for afp_layer_set_position | Complete scene lifecycle is separate |
| 0x1036BED0 string table / about 18747 | obj_normal_red/blue, obj_top_red/blue, obj_gold_red/blue and long_head names | Names exist in the original gmobject texture table |

Movie entries come from afp/afplist_Exml KBinXML. An afp entry has an original-encoding name and a u16 geo array. Movie members use MD5(name); BSI uses afp/bsi/MD5(name); geometry uses geo/MD5(name + "_shape" + id). A hexadecimal manifest name beginning with a digit gets an underscore prefix. Mapping was compared with ifstools 1.15.

## API and failure behavior

AfpPackage provides bitmap(name), movieBytes(name), bitStreamBytes(name), geometryBytes(name, shape) and original texture-sheet dimensions.

bitmap decodes and crops only in memory. Returned dimensions describe the visible UV rectangle, without border padding. For example, the normal red object is stored as 74 x 74 with a visible 72 x 72 UV region. This package API alone does not restore the movie anchor, scale, matrix or animation.

A missing bitmap, movie, BSI or geometry reference throws an error. A pure-image package may omit the movie table; a present movie table with missing dependencies fails explicitly. Unsupported opcodes are never reported as successful execution.

## Validation

rbafp GAME_ROOT reads original data without exporting media:

- 881 NGPF packages; the other IFS in the wider audit is a texture-only archive.
- 652 movies, 652 BSI streams, 7,608 geometry references and 8,478 visible bitmaps.
- 37,414 checks, including original texture-sheet dimensions and UV bounds.
- Observed movie signature 0xc1d0b208 and a second big-endian u32 matching member length. These observations do not interpret the rest of the header.
- tools/verify_afp.py: 74 independent bitmap/movie/BSI comparisons covering gmobject, gmframe_00 and title. Report: .build/afp-parity.json.

These are archive and pixel-reading checks, not complete scene or movie execution.

## Related work and limits

[AFP_TIMELINE](AFP_TIMELINE.md) adds BSI, rectangular GE2D, nested display lists, supported Switch stop/goto labels and original bitmap draws, including independent Switch/Bigbang/background samples. [STAGE_FRAME](STAGE_FRAME.md) adds the original bottom-panel rectangular mask, named texture bindings and independent child clocks. Full VM behavior, arbitrary scripts, general masks, nonrectangular geometry and original fonts remain unfinished. The local AFP library has no existing decompilation source. Static images are not used to claim completion of an original AFP page.
