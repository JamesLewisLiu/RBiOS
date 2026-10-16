# Original stage execution adapter

Local work and validation stay under `E:\MBR-2019121100\IDA`. No original game process, original DLL entry point, server transport, commit, or push is invoked. Assets are read in their original form; this stage does not convert them and does not persist results.

## Recovered execution order

`StageSession` joins the separately recovered `MusicSheet`, `Route`, `MotionTimeline`, `NoteFrame`, `TouchInput`, judgment dispatcher, CPU, and scoring modules. The stage clock advances in integer 60 Hz ticks from -150. Each tick follows `1015FC60`: input snapshot, pending pool sort/layer assignment, actor state update, judgments (`10160620`), judgment-triggered links (`1015E8C0`), initial source launches (`1015EE00`), then frame-end motion/countdown/deletion (`101577D0` / `1015AC00`). A source launch must occur on its exact prepared start tick. Allocation uses the original first free slot within the fixed 256-note pool; missing predecessors and invalid links fail explicitly.

A launch configuration is reconstructed from `100A3540` for initial objects and `100A3380` for judgment-created targets. `1015AB50` copies linked motion or builds the initial route. `1015AAD0` builds route geometry against chart opposite-side, and `1015A970` / `1015B060` implement dynamic Just Reflec redirection and active chain propagation. Top-entry timeline reset retains already-built route geometry. The original type-3 tempo command imports the float at wire +16; `bpmAt` follows ordered `10148E80` lookup.

`SessionOptions.endTick` is the **minimum completion tick**, default 1000 from `10149010` / global `1247930C`. It is not catalog +464. `1015EA10` also has a header duration +660-tick upper bound. The numeric adapter waits for all generated note slots to drain; the original end-presentation actor is not reconstructed by this class.

## Ordering and deterministic input

The shipped comparator `10157900` returns -1 for equal active ticks; host `std::sort` would therefore be an invalid substitute. `StageOrder` restores the shipped CRT `100302D0` quicksort / `10030240` selection-sort suffix, including tie ordering. `verify_stage_order.py` compares all identities against original isolated instructions for 1,000 arrays, mostly the complete 256-note pool and dense equal ticks. The same fixture also checks `1015A100` angle computation against 1,000 original finite vectors at explicit x87 control word 0x027f.

`TouchInput` uses original stage match distance 70. UIKit supplies already inverse-transformed coordinates, and the fixed-frame adapter performs original association and reuse. Pausing must freeze the platform clock; cancelling or changing the viewport calls `cancelTouches()`. Passing a target later than the current tick processes every intermediate tick with the supplied snapshot. For historical input changes the caller must submit their original tick boundaries, rather than replaying the newest snapshot over old input. `ios/StageInputTimeline.swift` now supplies this adapter: it maps UIKit boot-monotonic timestamps to floor((audioSeconds + eventHost - audioHost) * 60), emits old-input ranges before each event tick, clamps stale events to the next unprocessed tick, retains future snapshots, and collapses same-tick updates. Its 11 independent XCTest cases include equivalent inputs under different display-frame cadences; execution awaits an Apple Swift/Xcode environment.

## Snapshot and presentation boundary

Snapshots expose raw original graphic selection/draw parameters, note coordinates, direction, opacity, scale, stage tick, original creation tick, elapsed lifetime, and held-label transition tick. `direction` follows `1015A100`: two float-stored atan2 results, `atan2(-1, 0) - atan2(-dy, dx)`. The frame-end switch selector follows `1015AC00`: direction 0 chooses style 7 (`swich_on`), direction 1 chooses style 8 (`swich_off`). The optional `onJudgment` observer receives memory-only events and performs no storage/network work.

AFP/JR/hold/switch trail presentation requirements are explicit in `unsupportedPresentation`; a platform presenter must resolve these against its supported capabilities. This stage does not invent static pictures for unavailable animation. Full original AFP actor hierarchy, result/end animation, hit sounds and native audio-clock behavior are outside the portable numeric stage evidence. The iOS bridge handles native resources/presentation separately.

## Validation and limits

- Portable CI fixture: 120 BPM, original 20-second header, two side-specific source objects, original top-entry flag 0x10, original gauge initialization, local auto and perfect CPU. It checks creation, score 56/56, all slots drained, completion at minimum tick 1000, deterministic single-tick versus batched progression, cancellation and backwards-clock rejection.
- `verify_stage_order.py`: 1,000 original instruction comparisons, full identity order, plus 1,000 original direction comparisons.
- Existing component route/motion/frame/dispatcher/touch/CPU/score tests establish their own isolated boundaries; this is **not** a claim that the entire original stage has been differentially emulated.
- Real catalog audit on both chart sides completed 2,309 original charts × two sides × two local input modes (9,236 complete song sessions), with original CPU difficulty 0; see `.build/stage-audit.json` and `.build/stage-audit-side0.txt` / `stage-audit-side1.txt`. All prepared objects were generated, all pools drained, all local automatic totals matched, and CPU omissions were restricted to verified short-hold missed heads.
- `StageSessionTests <resource-root> <limit> <side>` audits original catalog-referenced charts in local auto and no-contact manual modes, both against original CPU level 0. It checks every prepared object was generated and no slots remain active, and local auto event totals equal the original sheet counters. CPU missing events may only match the explicitly proved short-hold boundary below; unrelated count differences fail.

### Original short-hold missed-head boundary

For duration 1 through 11 ticks and no initial touch/CPU result, the original exit state reaches completed before initial miss can occur (`relativeTick < -9`). The dispatcher then enters its completed-hold branch and eventually reports only the tail miss (`grade 4`, reason 6). The original sheet total still includes a head. `verify_short_hold.py` runs original `10159B30`, scheduler `10008D60`, fade `10159AF0`, real miss query `10158550`, and dispatcher `1016B940` for all 11 durations. It does not stub the miss result; only the first-press query reports the explicit absence of touch. This exactly explains observed per-object omissions on real short holds, including objects 189/191/355 in `3k08_twinklecircle_h.ply` (171 ms -> 10 ticks). The adapter preserves this behavior rather than adding a fabricated score event.

Real-device touch, rendering, timing and online-server behavior require separate validation. No server connection is part of these tests.
