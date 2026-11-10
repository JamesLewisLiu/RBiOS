# Validation record

Updated 2026-11-10. Host: Windows; Apple builds run on macos-15. Original-resource analysis tools, caches and results stay under the original analysis directory. Isolated original-function fixtures, native integration audits, Apple builds and device play are distinct evidence.

## Host and original-instruction checks

| Check | Passed result | Scope / limit |
| --- | --- | --- |
| Zig 0.13 C++17 | 25 test programs | Component assertions plus lifecycle scenarios; FMA contraction disabled |
| Python Mach-O checks | 9 tests | Synthetic arm64, legacy/universal, truncation and wrong-platform cases; not Apple compilation |
| English repository text | No literal Han characters | Authored text only; original resource bytes and runtime metadata preserved |
| Documentation and project structure | Local file links, Xcode project, plist, XML and JSON checked | Structural checks are separate from Apple compilation |
| Baseline chart/timing | Seeds 36, hold 15, Switch 32, classification 560, timing 424 | Bounded isolated original instructions; fixed DLL hash |
| MusicSheet original loader/prepare | 12 cases,169,932 fields | Three real charts x both sides x load/prepare; default option, explicit allocation fixture |
| Full chart preparation | 2,315 charts,1,818,146 objects | Includes files not enabled in the selectable catalog |
| Motion | 1,090 cases | Float-bit geometry/timelines; supplied routes and explicit callback boundary |
| Route | 8,600 cases | Original scalar D3DX, seeds, chart config and redirection; other SIMD dispatch unverified |
| NoteFrame | 4,500 complete 448-byte states | Original state functions; external hooks retain declared fixtures |
| Touch queries | 3,690 cases | Real default-field geometry; UIKit/hardware delivery excluded |
| TouchInput | 3,000 frames | Association, recycling, exhaustion and ID wrap |
| CPU | 3,478 cases | Original random/distribution/shuffle/gates; explicit seed/difficulty |
| Score | 2,007 cases | Numeric life/gauge/combo/rates/results; no result scene |
| JudgeDispatch | 10,000 cases including 2,000 replay | Explicit touch-query fixtures; query internals compared separately |
| Gameplay | 1,500 cases | Original state/filter/score/bonus; AFP actor virtuals stubbed |
| Stage initialization | 600 complete 172-byte cases | Original 101554C0 and explicit offline configuration |
| Stage sort/angle | 1,000 full identity orders and 1,000 angles | Original CRT tie behavior and float bit patterns |
| Short-hold boundary | Durations 1..11 ticks | Original scheduler/miss/dispatcher proves absent missed-head events; no fabricated replacement |
| Complete-song numeric/lifecycle | 9,236 runs | 2,309 charts x both sides x auto/no-contact with CPU0; all objects created/drained; auto totals match |
| Long graphics | 420 original draw cases | Ordinary 180/Switch 240;1-16 segments; original parameter capture |
| Full long-note connection | 62,563,101 frames,162,026,891 draws | Create/release each 7,252,200; both 4,618-row numeric logs unchanged |
| AFP archive/UV | 881 packages,8,478 images,37,414 checks | Original movie/BSI/GE2D bindings and visible UVs |
| AFP independent reading | 74 comparisons | Original bitmap/movie/BSI bytes |
| AFP independent timeline | 2,040 fixed frames and 798 label samples | Includes 1,440 background frames; fixed reference commit; not full VM |
| AFP pool/clock | 1,031 host,862 descriptor/instruction,6 hidden-clock checks | 37 original descriptors and persistent reuse clocks |
| AFP within-note cleanup | 68 checks,56 actual releases | Original component/actor/pool reset; cross-note order excluded |
| Stage graphics | 56 checks | Original line/Top geometry and actual referenced images |
| Presentation pipeline before frame integration | 2,309 charts,522,082 display samples,22,085,142 draws | Every-tick movie control, stride 30 drawing,131 original bitmaps; explicit missing effect queues |
| Stage-frame independent comparison | 908 samples,9,080 draws,6,356 masked draws,56 timer textures | Four difficulties, two child cycles and large elapsed times; upstream decoder reference, not original game render trace |
| Frame-integrated chart sampling | Four charts,738 samples,36,542 draws,139 bitmaps | All four difficulties; full catalog drawing audit not repeated |
| Targeted every-tick draw with frame | 6k60_lastboss_w.ply:7,498 frames,635,983 draws,195 bitmaps | 70 Switch,34 Bigbang,58 hold objects; not GPU pixel verification |

New route/frame/sheet/input/order suites explicitly set x87 CW=0x027f. Older suites retain their documented fixture environment. The live original process control word has not been observed.

## Resource and protocol checks

| Check | Result | Boundary |
| --- | --- | --- |
| Full resource reading/decoding | 0 errors:2,315 charts,1,818,146 objects,882 IFS,8,478 textures,1,621 audio,945 catalog entries | Prior full decode audit; unchanged decoders |
| Texture reference | 6 samples | ifstools/Pillow BGRA including compressed/raw branches |
| Audio reference | 6 samples | FFmpeg PCM16, including real5z06_erinvenus.2dx; fact padding trimmed |
| KBinXML reference | 52 synthetic trees | kbinxml2.2 CP932/UTF-8, names and typed values; not EA3 envelope |
| Original network fields | 8 read cases,1 score-read structure,1,152 callback states | Explicit AVS allocation/string fixtures; not live protocol |
| Resource snapshot | 4,834 files match | File set, sizes and SHA-256 since implementation-time snapshot |
| Character bitmap completeness | All 86 original packages contain one 768 x 1360 bitmap | First character's edge alpha and visible BGRA hash checked; cut edges are in original artwork |
| Apple frame build at 9cbc657 | Unsigned arm64 archive, minimum OS 14,21 simulator tests | [Actions 42052166298](https://github.com/JamesLewisLiu/RBiOS/actions/runs/42052166298); GPU corners/judgment lines at 50/75/100%, adapter/audio/input tests |
| Nested-mask GPU regression | Passed at 50/75/100%, no test skips | Synthetic marker intersection through the production renderer, not original scene pixel parity |
| Device observation | User installation and stage-completion screenshots | Corrected playfield framing; new stage frame, timing and performance still need device validation |
| Actual server | Not contacted | No probe, login or upload |

The full decode audit processed 3,530,137,320 stereo audio frames and 1,128,995,636 texture bytes, one file at a time. These totals are coverage counters, not peak memory/performance measurements.

The frame-build IPA SHA-256 is `bd9c5808eeaadf2af2825ee816b72f79683bb9adca857cc9eb4264e0d9794adc`. Its Info.plist and arm64 Mach-O both specify minimum OS 14.0. The post-implementation resource snapshot check again matched all 4,834 original files.

## Reproduction

Portable checks:

~~~powershell
python -B tools/build_host.py
python -B -m unittest discover -s tests -p 'test_*.py'
python -B tools/check_repository_text.py
~~~

Original analysis checks require the matching game files and optional dependencies inside .tools:

~~~powershell
python -B tools/verify_original.py
python -B tools/verify_motion.py
python -B tools/verify_touch.py
python -B tools/verify_cpu.py
python -B tools/verify_protocol.py
python -B tools/verify_score.py
python -B tools/verify_dispatch.py
python -B tools/verify_gameplay.py
python -B tools/verify_sheet.py
python -B tools/verify_route.py
python -B tools/verify_frame.py
python -B tools/verify_touch_input.py
python -B tools/verify_stage_score.py
python -B tools/verify_stage_order.py
python -B tools/verify_short_hold.py
python -B tools/verify_long_graphic.py
python -B tools/verify_long_graphic_assets.py
python -B tools/verify_afp.py
python -B tools/verify_afp_pool.py
python -B tools/verify_afp_release_order.py
python -B tools/verify_afp_layer_state.py
python -B tools/verify_stage_frame.py --help
python -B tools/verify_character_resources.py --help
python -B tools/audit_presentation.py ..
python -B tools/verify_readers.py
python -B tools/verify_property.py
.build/rbinspect.exe ..
python -B tools/hash_original.py --verify
~~~

Detailed JSON/logs remain in ignored .build. The committable summary is [local-results.json](validation/local-results.json), without decoded media.

Existing function suites preserve their fixture limits. New full-chart/scheduling/presentation checks are native deterministic audits, not execution of the entire original game. Host integration cannot establish Apple audio synchronization, original per-song replay scores, GPU pixels, device performance or real-server compatibility. Simulator rendering checks and user screenshots provide separate, narrower evidence; they do not establish original-engine pixel parity.

## Score presentation restoration, 2026-11-10

Original score/combo draw instructions passed 96 cases / 1,272 draws. The displayed-score update passed 2,400 original-instruction comparisons. Integrated chart rendering passed four difficulties with 738 samples / 45,289 draws / 192 original bitmaps. The Official IDA MCP Server freshly decompiled the related routines; see [STAGE_SCORE_GRAPHIC](STAGE_SCORE_GRAPHIC.md) and [SCORE_DISPLAY](SCORE_DISPLAY.md). These are bounded instruction and resource checks; the new presentation still requires device testing.

## Normal score labels and digit boundary audit

The original four score-label textures and AFP positions pass independent resource comparison. Integrated tests require both labels in every sampled frame: four difficulties, 738 samples, 46,765 draws and 194 bitmaps passed. The expanded original score/combo instruction comparison passes 120 cases / 1,704 draws, including large-value truncation. It confirms the existing four-digit cap and corrects the earlier documentation. See [STAGE_LABEL_GRAPHIC](STAGE_LABEL_GRAPHIC.md).
