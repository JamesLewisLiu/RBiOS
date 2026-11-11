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

## Local effects and display cadence, 2026-11-10

All 26 native test programs and nine Python tests pass locally. The sound selector passes 16,800 comparisons against the original instruction chain and seven SD9 identity checks. All seven effects decode through the native Wave reader to 1,440,928 bytes of non-silent, stereo PCM in memory. The batched StageSession test confirms one local sound, no synthesized rival sound and 1,151 per-tick drains.

Four original chart difficulties pass with 738 sampled frames, 46,765 draws and 194 used bitmaps. Before each run, all 671 textures from the five supported stage packages decode successfully (47,597,024 BGRA bytes). No source assets are converted or replaced. Apple build/simulator results are tracked separately through Actions; actual device audible output, frame times and latency are unverified for this increment.

## Menu flow and continuous held-note sound, 2026-11-11

The recovered difficulty controller passes 24,576 original-instruction comparisons. The continuous-sound controller passes 153 native assertions and all four original default-bank lifecycle combinations. Eight synthetic Hold/Switch sessions compare every batched/per-tick state; additional manual contact, cancellation, rival-silence and overlapping-note checks pass. The original loop decodes to 51,772 PCM bytes in memory. The four original difficulty voices match the source table and SD9 identities and decode to 2,544,264 PCM bytes in memory. All 30 native test programs and nine Python tests pass locally.

Three original menu pages pass with 195 draws (156 textured, 39 solid), including direct checks of the original title white rectangle. The unchanged stage audit passes four difficulties, 738 sampled frames, 46,765 draws and 194 used textures after preparing all 671 stage textures. These checks establish supported asset parsing and bounded logic; full original page scripts, native page interaction on the device, loop audibility and original-engine pixel parity remain separate.

Simulator coverage adds difficulty availability/debounce/confirmation, native control wiring, result counter mapping and untextured solid-rectangle GPU rendering at every display scale. See the associated Actions run for Apple results; locally generated host results alone do not establish Apple correctness.

## Original execution recovery foundation, 2026-11-11

The replacement page flow is no longer accepted as original-runtime migration. A preserved local branch, `codex/component-harness-c9cf3ae`, identifies the earlier playable harness. Its continued compilation is not evidence of AVS boot.

New bounded original-code comparisons pass: 6,120 TEST navigation/exit-flag cases and both complete menu tables; 512 game-entry identity cases; 256 AVS product-mode cases comparing all 2,528 configuration bytes and correction logs. No scene success, AVS service success, server traffic or NVRAM writes are synthesized. The modules are not wired into a pretend successful bootstrap. Full service initialization, actual TEST subscene dispatch and original gameplay ownership remain outstanding.

## AVS import/mount and TEST wait recovery, 2026-11-11

All 36 portable C++ test programs and nine Python tests passed locally. Final binaries passed 281 original AVS configuration comparisons, 960 filesystem-dispatch comparisons and 7,698 TEST wait-state comparisons. The configuration oracle compares all 2,528 bytes, including actual original XML values prepared as an offline typed fixture. The defaults-only case executes original instructions without replacements; override cases replace only property path lookup. Filesystem comparisons capture actual original dispatch instructions with backend calls intercepted. TEST comparisons replace only the two external EA3 operations.

A native data-contract check composes typed property import, product-mode normalization and mount request capture in both product modes. This does not execute descriptor/thread/heap initialization between those phases. The app still uses the existing component harness; full AVS/EA3 services, original scene ownership and TEST rendering are not newly enabled. No resource conversion, NVRAM write or game-server traffic occurred. Apple compilation and simulator checks are reported by the corresponding Actions run, separately from local evidence.

## Native AVS service recovery, 2026-11-11

Original comparisons passed: 3,401 file-path normalization/hash cases without hooks; 533 property-parser cases and 20 complete property-search lookups; 18,244 descriptor lifecycle commands including 768 bulk cleanup calls; 400 original vector-read comparisons against generated real host files; 803 XML metadata comparisons without hooks. The boot importer still passes all 281 image comparisons after integration with recovered property search. Each component guide identifies its substituted environment and unsupported domains.

Cabinet configuration tests write only generated fixtures beside the host executable. They verify first-version backup retention, byte-preserving replacement, missing directories, disallowed paths and failure before replacement when backup storage is unavailable. These are native storage-policy tests, not original XML serialization evidence. No original resources or configuration were changed. Complete boot, device startup and live server compatibility remain pending.

Final Windows service build passed all 42 C++ test programs and nine Python tests. Apple/ARM64 build results belong to the corresponding Actions revision and remain separate from these local checks.

## Original startup prefix and concrete service dependencies, 2026-11-12

All 47 Windows C++ test programs pass. Original-instruction comparisons pass for 56 complete XML documents (42 trees and 14 rejected inputs), 2,004 mutex ownership commands, five main-thread priority inputs, 500 mount/path mappings and 1,259 mount option strings. The XML oracle executes original property_create and insert_read, substituting only its input reader; original configuration and CP932 bytes are included. Other component guides record OS callback substitutions and unsupported boundaries.

Apple tests cover the new startup adapter and root selection. At bb19e63, [Actions 43087472168](https://github.com/JamesLewisLiu/RBiOS/actions/runs/43087472168) passed 47 portable C++ tests, nine Python tests, the arm64 archive/minimum-OS check and all 48 simulator tests. Matching IPA/TIPA packages were produced. The default runtime deliberately stops at the incomplete thread bootstrap. Neither this prefix nor independent service checks establish AVS initialization, original TEST, device play or live network compatibility.

## Native worker recovery, 2026-11-12

All 49 Windows C++ test programs pass after integrating bootstrap policy and native workers. Original comparisons pass for 1,024 bootstrap configurations, 36 affinity sequences, 17 pool constructions and 74 task transitions. OS operations are captured in the original oracle; the native comparison uses real worker threads. Assignment targets an explicitly selected slot, so the oracle does not claim complete original task admission or retired-list recycling.

Native failure tests cover callbacks, task exceptions, tasks returning or throwing while holding recursive locks, preservation of another thread's ownership, self-wait/self-join rejection and stop while work is executing. These are native exception-containment checks, not substitutes for missing original task behavior. Default startup remains stopped before full AVS boot. Apple verification for this increment belongs to its subsequent Actions revision.

## Finite tasks, resource pools and alarms, 2026-11-12

All 52 Windows C++ test programs pass after integrating the latest services. Resource pools pass 18,894 original allocator/free/query commands. Worker admission passes 48 configurations and 4,800 checkout/recycling operations, plus two original export-091 descriptor-exhaustion cases. Alarm record and dispatch state passes 254 comparisons, including 64-bit periods and repeated raw-clock wrap. Each oracle documents its OS/caller fixtures; none is a complete original-process execution.

Native regressions also cover externally pooled mutexes, deferred descriptor owner teardown, stale-token rejection after payload reuse, admission during shutdown and bounded alarm semaphore wake counts. These failure-containment checks are separate from original-instruction comparisons. The alarm oracle's synchronization hooks do not validate the native semaphore limit or scheduling.

The previous worker revision 9056cb0 passed [Actions 43106827921](https://github.com/JamesLewisLiu/RBiOS/actions/runs/43106827921): 49 portable tests, nine Python tests, an arm64 archive with minimum OS 14.0, and all 48 simulator tests. Its matching IPA/TIPA artifact is retained by Actions. The current service revision has its own Actions record; Windows results alone do not establish Apple correctness. No device gameplay, full AVS boot or live server compatibility is claimed.
