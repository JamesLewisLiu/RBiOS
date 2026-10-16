# Judgment dispatch and submission

The base native call chain is original touch/CPU -> judgment dispatcher -> note state submission -> reason filtering -> scoring -> terminal combo bonus. [STAGE_SESSION](STAGE_SESSION.md) adds complete-chart scheduling. Apple audio-clock integration is written but not yet run on an Apple platform.

| Original address | Restored interface and boundary |
| --- | --- |
| 0x1016B940 | dispatchJudgment: original modes, ordinary/hold/Switch and held/contact state |
| 0x1016AC80..0x1016B740 | Auto, touch, CPU and replay helpers, lazily queried in original order |
| 0x10159E10 | applyBaseNoteEvent: replay cursor, initial hit, hold end, segment advance and direction change |
| 0x10159A20/50/80 | Base-note initial/finish state |
| 0x10158B40 / 0x10158AD0 / 0x10158330 | Link removal, input block and associated-note cooldown; some AFP side effects remain |
| 0x1015F8C0 | consumeJudgment: state first, then reason 7/8 filtering; only grade 3 converts to scoring reason 4 |
| 0x1015F6E0 | Final combo bonus, remote-result override calculation and medal; medal scene not restored |
| 0x10160620 | evaluateNote and optional local grade 1->2 rule; pool traversal is connected in StageSession |

RuntimeNote synchronizes typed views at original boundaries. evaluateNote connects real TouchState, NoteTouchState and CPUState to dispatch. CPU shares an explicit OriginalRandom. Original geometry 0x101584B0/0x100A4F80 is used; derived notes may override explicitly.

Absent CPU, absent replay and an empty replay record remain distinct cases. Missing required CPU RNG or Switch data throws rather than returning success. Linked cooldown propagation detects cycles without imposing an arbitrary short-chain limit.

Original 0x1015DE90 binds CPU+4 to the current score. 0x1015DB50 reads its +100 JR meter; the adapter synchronizes this immediately before CPU invocation, retaining probability timing and shared RNG order.

Display size changes only the viewport/inverse transform. InputRules.touchScale is original hit-box configuration, independent of 50%-100% screen scaling.

## Validation

- tools/verify_dispatch.py: 10,000 original dispatcher cases, including 2,000 replay cases. Original record lookup executes; allocation is bounded fixture storage and touch query results are explicit fixtures.
- tools/verify_gameplay.py: 1,500 base-note/state/filter/full-score/bonus comparisons. Original score/state instructions execute; AFP virtuals are stubs and actor pointers are absent.
- GameplayTests: 19 checks with actual migrated touch/CPU/dispatch/submit/score, including first press, hold filtering/grace, Switch and final bonus.

MusicSheet, Route, NoteFrame, TouchInput, StageScoreInitialization, StageOrder and StageSession now connect preprocessing, source/linked launches, 60 Hz updates and session completion. RBGameSession and GameViewController supply the Apple entry. Normal play follows samples; preroll and post-audio tail use a pausable monotonic clock.

[GAME_PRESENTATION](GAME_PRESENTATION.md) connects original Switch/Bigbang/background and hold/Switch geometry. Special effect consumers, original pages and hit sounds remain unfinished. Host complete-song audits do not establish original replay or device equivalence. Leaving the session discards results; no automatic upload occurs.
