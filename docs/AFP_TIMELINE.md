# AFP exported-symbol runtime subset

`rb::AfpTimeline` reads original AFP/AP2 movie bytes with their original BSI swap stream. Original `.ifs`, texture data, movies, BSI and GE2D files remain unchanged; decoding and frame sampling happen in memory.

## Implemented and verified

- BSI numeric byte swaps, AP2 v7–10 / data version 0x200 headers, obfuscated strings, exports, nested sprite definitions, frame labels and display lists.
- Rectangular GE2D textured triangle pairs with original region names, vertices, placement matrices, rotation origins, RGBA multiplication and blend modes. All four GE2D UV points must match the named original bitmap uvrect normalized by the original sheet dimensions; mismatches fail explicitly.
- Partial matrix/color updates retain unchanged original components. Transparent original dummy geometry is omitted only when effective alpha is zero.
- Minimal AP2 stack bytecode for original `aeplib.stop(this)`, `aeplib.deepGotoAndPlay(this,label)` and `aeplib.aep_set_set_frame(this,frame)`. Frame callback metadata is decoded from original ON_LOAD events.
- `sampleExportedFrame()` seeks a **zero-based display-list frame index**. It validates known frame scripts but intentionally does not apply script jumps during an explicit seek.
- `sampleExportedLabelFrame()` uses original **one-based movie frame labels**, starts at `label - 1`, executes supported frame stop/goto actions, and resolves a goto in the same tick. `elapsed` counts animation ticks since the external label change. Stop persists; goto restarts playing. Repeated resolved playback frames detect cycles and skip complete cycles, bounding long-label sampling cost. Unknown labels/actions and same-tick jump cycles produce errors.

Verified package: original `data/graphics_rb6/gmobject.ifs`. Four `obj_bigbang_{big,small}_{red,blue}` exported symbols each have 120 frames; both `swich_on` / `swich_off` symbols have 60 frames. Switch original labels are `off=1`, `on=4`, `loop=5`. Stop occurs in frame index 2; deepGotoAndPlay(loop) occurs in frame index 45. Thus an `on` start initially samples index 3, then index 4 one tick later; at elapsed 42 the goto has returned to index 4. These values come from the original data, not an invented animation.

`tests/AfpTimelineTests.cpp original-gmobject.ifs` passes 3,221 checks. `tools/verify_afp_timeline.py` independently compares 600 fixed-frame samples / 1,364 draw commands plus 798 label playback samples / 2,257 commands against the pinned reference decoder. Texture names, all six matrix values, all four bounds, all four texture UV points, RGBA multiplication and blend match (absolute float tolerance 0.0002). Test reference source is kept under `.tools/afp-reference`; output is `.build/afp-gmobject-timeline-parity.json`. No resource images are exported or converted by this test.

Also verified all six background exports: `gmbg_00.ifs`, `gmbg_01.ifs`, `gmbg_02.ifs`, each exporting `gmbg_00` and `gmbg_01`, each 60fps / 240 frames and location `[0,768,0,1360]`. Independent comparison covers all 1,440 frames and 4,800 draw commands. Run the same verifier with `gmbg_00`, `gmbg_01`, or `gmbg_02` as its optional argument; it writes per-package reports. These are original rolling/covering placements, not stretched background bitmaps.

## Renderer contract

Both `location` and `AfpSpriteCommand.bounds` are `[left, right, top, bottom]`. Coordinates use AFP's top-left origin with Y increasing downward. Matrix storage is `a,b,c,d,tx,ty`, and maps each original geometry vertex `(x,y)` as:

```
x' = a*x + c*y + tx
y' = b*x + d*y + ty
```

The composed matrix already incorporates each placement's original rotation origin and ancestors. Apply it to the bounds' four corners, preserving original texture orientation. To center a movie at a game position, subtract the movie `location` center before applying the caller's external rotation/scale. Geometry bounds are not texture pixel dimensions and should not be replaced by bitmap dimensions.

Blend 0 is normal, 8 is addition. Every tested Switch/Bigbang draw has zero additive RGBA. Nonzero additive color currently throws, so a bridge must report that error rather than discard color data. Draw commands retain original depth order. A caller must add its own original game layer ordering.

Original `reflecbeat.dll` 0x10155A00 calls 0x10066430 to check the movie's current frame against the `on` label before setting it; held `on` does not restart each draw. Its released path repeatedly sets `off`; the original script stops that path. Use `prepareSwitchMovieDraw()` with a `SwitchMovieState` per original pooled movie instance; it performs the repeated off seek and the conditional on seek, and preserves hidden clocks. Do not reset a reused instance solely because another chart object acquires it.

## Original layer clock evidence

`tools/verify_afp_layer_state.py` runs isolated original x86 instructions from `libafp-win32.dll`, SHA-256 `9542ded21dd85474b8323a36ff019bc5dbae8717d903295c32e36a52b0c93d08`. Six checks pass: afp_layer_play (0x10004D50) sets layer+0x2C playback rate; afp_layer_set_attribute (0x10007050) changes visibility bit 1 without altering that rate; the original advance routine 0x100071F0 reaches its pre-VM advance boundary for both visible/hidden layers at rate 1, but not rate 0. The valid layer lookup and initialized-engine flag are explicit fixtures; execution stops at 0x10006A00 before the full movie VM. Disassembly additionally confirms afp_render_group0x10007120 advances layers without filtering visibility, while afp_display_all0x100079C0 filters bit 1 at 0x10007A46.

Pool creation evidence: 0x10164A10 calls 0x10067130→0x10067000→0x10066C30. Creation seeks frame 0; then 0x10029810 sets rate 0; then SetVisible 0 hides each movie. `0x10155F10` acquires existing pool entries without resetting them. Pool release 0x10164BC0 only hides and returns the instance; it does not stop or seek it. The helper defaults therefore describe a freshly created movie, and its state must survive actual pool reuse.

Therefore Switch must retain separate movie clocks for `swich_on` and `swich_off`: hiding the other instance in 0x10155A00 does not stop it. Continue elapsed time while hidden, and reset only for an actual SetFrameLabel on that instance. `sub_100299A0` tests layer_get_info playback rate (offset 8), not whether a nested script has called stop. Its fallback Play 1 does not seek/restart the movie.

## Original movie pools

`tools/verify_afp_pool.py` reads the 37 original descriptors at 0x1036BFD8 (28-byte stride) and runs isolated original 0x1001EF40 / 0x10164C30 / 0x10164BC0 for initialization, acquisition and release. 862 checks pass, including all 37 C++ descriptors compared field-for-field to the original DLL. Complete descriptors and test evidence are saved to `.build/original-afp-pool.json`. The DLL is bound to the same REFLEC SHA-256 recorded above; the virtual SetVisible operation is a recording fixture.

| Pool index | Original symbol | Capacity | Game layer |
|---|---|---:|---:|
|23|swich_on, local side|64|40|
|24|swich_off, local side|64|40|
|25|swich_on, opponent side|24|40|
|26|swich_off, opponent side|24|40|
|27|swich_long_body_on|64|62|
|28|swich_long_body_off|64|62|
|29|long_tail_bg_switch|24|40|
|31|obj_bigbang_big_red / blue|24|40|
|32|obj_bigbang_small_red / blue|24|40|

All listed packages are original package 33 (`gmobject`). Bigbang's red/blue alternative follows the configured color for the relevant side. First-free order initially ascends slot 0,1,2...; allocation pops the linked-list head and clears its next pointer. Release pushes the instance to the free-list head, giving LIFO reuse. It calls only SetVisible 0 before returning the entry. Reacquisition does not seek the movie. Keep `SwitchMovieState` keyed by original `(poolIndex, slotIndex)` through different chart objects, and acquire/release on original session lifecycle callbacks rather than deriving lifetimes from visible sprite snapshots. `OriginalAfpMoviePool` implements these 37 original pools; its `acquire()` returns an optional slot, `release()` hides and returns it, and `state()` preserves `SwitchMovieState` throughout reuse. The standalone test exhausts and reuses every original pool and checks both error handling and clock retention. Original exhaustion returns a null movie; it is not permission to invent additional live instances. The original global diagnostic allocation count nevertheless increments on failed acquisition, an observed quirk the test does not hide.

## Explicit limits

This is an exported-symbol renderer, not the complete AFP VM. Global movie imports/initializers, global aeplib registration, arbitrary bytecode, conditional execution, nested script-controlled animation, masks, nonrectangular meshes, 3D/projection, texture UV variations, nonzero additive colors and other unsupported options report errors. Nested un-scripted sprite time advances from its placement tick. The targeted Switch exported symbols use the supported scripts directly and original rectangular image placements; Bigbang uses supported nested display lists. There is no replacement artwork or static image substituted for an unsupported animation.

The format interpretation and one-based frame API are corroborated by the upstream implementation. No full movie execution trace of the proprietary AFP DLL or iPad rendering has yet confirmed timing against the original engine. These tests validate decoding/parity with that reference, not whole-game or true-device equivalence.

## Primary reference and license

Format reference: [DragonMinded/bemaniutils, commit 1e66fcd780afb3088ce8e78c88615a143db492ee](https://github.com/DragonMinded/bemaniutils/tree/1e66fcd780afb3088ce8e78c88615a143db492ee/bemani/format/afp), especially `swf.py`, `geo.py`, `types/ap2.py`, and `render.py` (`PlacedClip.gotoAndPlay` and `Renderer.__process_tags`). The reference is Unlicense; its license is preserved at [licenses/bemaniutils-UNLICENSE.txt](licenses/bemaniutils-UNLICENSE.txt). The application does not import the Python reference at runtime.
