# Execution fidelity audit

Audit date: 2026-11-11. Baseline: commit `c9cf3ae`. This is a source and existing-evidence review, not a fresh execution trace of the original game. Concurrent bootstrap recovery work is outside this baseline.

## Finding

The delivered application is an offline harness around recovered game components, with a newly authored page flow. It does not restore the original AVS bootstrap, module-loading lifecycle, original scene controller or TEST menu. Using original textures, sounds and chart files does not establish execution fidelity. The user's corrected requirement excludes presenting the harness as the migrated original game.

This does not mean every gameplay algorithm was invented. Several numeric modules have address-bound comparisons against isolated original instructions. Those modules should be retained with their evidence boundaries. Native platform adapters are necessary on ARM64/iPadOS; the problem is replacing unported game decisions with new decisions and treating the resulting application as the original execution chain.

## Current application path and bypasses

| Baseline source | Observed behavior | Classification |
| --- | --- | --- |
| `ios/AppDelegate.swift` | Creates a UIKit navigation controller rooted at `LibraryViewController`. | Newly authored application entry; no AVS boot. |
| `ios/LibraryViewController.swift` | Imports a resource directory, reads the catalog and pushes native title/difficulty pages. | Resource browser and harness navigation. |
| `ios/MenuViewController.swift` | Supplies title/start, searchable song table, difficulty controls, result labels and replay transitions. | Replacement page controller. None of these complete page transitions are established as recovered original scene code. |
| `core/src/MenuPresentation.cpp` | Samples fixed title/music/result AFP frames, hides selected card/payment/profile placeholders and returns the cached frame regardless of tick. | Original static artwork under an explicit adapter policy; not the original attract/selection/result runtime. |
| `ios/RBNativeMenu.mm` | Supplies imported chart availability to a recovered difficulty controller and native button actions; dispatches exact recovered difficulty voice IDs. | Mixed: recovered small controller and sound table, but newly supplied eligibility/input/page context. |
| `ios/RBGameSession.mm` | Reads a chart and constructs `MusicSheet`, `GamePresentation` and `StageSession` directly. | Bypasses original game initialization, loading scenes and scene-owner state. |
| `ios/GameViewController.swift` | Advances the stage from a platform audio clock and custom preroll/tail clocks, then replaces the controller with a native result page. | Platform timing integration and replacement completion/page transition; not a recovered original main loop/end actor. |
| `ios/SettingsViewController.swift` | Offers viewport/server configuration and resource preview. | Application settings, not the original TEST menu. |

The baseline does not invoke recovered equivalents of `dll_entry_init` (`0x10168840`) and `dll_entry_main` (`0x10168AA0`) as its normal entry path. The original catalog DLL is version-checked and read as data. Not executing the x86 DLL on ARM64 is expected for a native port; failing to recover its entry semantics is the actual missing behavior.

No production startup path was found in the baseline that restores AVS configuration/mount construction, AVS service initialization, EA3 boot ordering, original module initialization, service-error handling or TEST dispatch. Existing property/asset readers and protocol models are components, not that lifecycle. The historical Windows startup log mentioned in `MIGRATION.md` is evidence of a prior original run, not evidence that the iPad app executes the same boot chain.

## Gameplay fidelity boundaries

| Area | Existing evidence | Adapter policy or remaining gap |
| --- | --- | --- |
| Chart parsing/preparation | Original `0x1014B870`, `0x1014A230` and related routines; format and complete-field comparisons described in `MIGRATION.md`/`MUSIC_SHEET.md`. | Direct chart construction bypasses the original loading/configuration owner. Full original resource-manager lifetime is absent. |
| Route, motion and actor updates | Address-bound route/motion/frame comparisons, including `0x1015A5E0`, `0x1015AC00` and original sorting routines. | `StageSession` connects separately recovered components; this is not a full differential trace of the original stage/actor hierarchy. |
| Input and CPU | Original touch association, judgment dispatcher and CPU logic have isolated instruction evidence. | `SessionOptions.inputModes` defaults to local manual/opponent CPU (`{0,3}`); CPU difficulty defaults to zero. Original mode/CPU selection through configuration/profile/scene state is not restored. UIKit sampling and audio-clock timestamp mapping are platform adapters. |
| RNG | Original RNG algorithm and call sites are preserved within the recovered subset. | `RBGameSession` chooses the initial seed with `arc4random()`. Original seed lifecycle across boot/scenes is not established. |
| Scoring | `initializeStageScore` restores `0x101554C0`; `StageScoreInitialization` defaults derive from `0x1007FFD0`. Judgment and score routines have isolated oracle comparisons. | The bridge always chooses `StageScoreInitialization::offline`; account/options/rule overrides are not resolved by original state. `StageScoreState` defaults such as stage mode, gauge mode and clear option supply one path rather than restoring every original caller configuration. |
| Completion | `endTick=1000` is documented as the original minimum gate from `0x10149010`/global `0x1247930C`; duration+660 is documented under `0x1015EA10`. | These constants must not be mislabeled as fabricated. The current completion boolean combines drain/timeout checks without the original end-presentation actor, then drives a native result transition. |
| Remote play/replay | Some dispatcher and score paths accept explicit replay/remote data and have bounded tests. | `StageSession` installs `acceptRemoteRoute = [] { return false; }`; normal app execution supplies no remote/replay session. Replay association effects remain excluded. This is explicit offline selection, not restored network/game-session decisions. |
| Just Reflec | Route redirection and relevant numeric state are present. | JR overlay and Switch JR effect queues remain reported as unsupported; full original effect consumers are absent. |
| Audio | Default local one-shot selection, suppression, held-loop aggregation and difficulty voice IDs have original-instruction/data evidence. | Rival audio stream, other profiles, complete menu/attract sounds and mixer/lifecycle behavior remain incomplete. AVAudioEngine scheduling is a platform implementation with device latency/parity still unverified. |
| Rendering | Original AFP data, selected movie clocks, glyphs and frame geometry are decoded in memory; bounded independent resource comparisons exist. | `AfpTimeline` supports a VM subset and does not execute movie-global initialization. Hidden dynamic bindings, unsupported geometry/scripts and native text remain gaps. 120 Hz interpolation adds visual delay and is an explicit presentation adaptation, not original simulation. |

The 9,236 complete-song runs cited by prior documentation establish properties of the recovered native harness over the catalog (creation, draining and selected event totals). They are not 9,236 original-game/native differential replays. Likewise, passing Xcode, simulator tests and resource hashes establishes build/adapter/format properties, not full original game equivalence.

## Networking and persistence

`core/src/Network.cpp` implements a generic injected transport and backend lifecycle. `core/src/Protocol.cpp` restores selected original fields/callbacks. `ios/RemoteTransport.swift` is dormant and defaults to rejecting live network access. These are not a recovered AVS/EA3 boot/discovery/card/profile/game-session chain.

The baseline does not fake a successful live login: offline exchange and unavailable embedded services throw explicit errors, and the app does not construct a live transport. The fidelity issue is that the offline harness bypasses those original decisions entirely. The saved server configuration must not be described as active integration.

User-requested absence of score/profile/history persistence remains a separate constraint. It does not prove that all original operator configuration, AVS mounts or TEST settings are safe to discard. Any port of those systems needs explicit original semantics and a clear separation from prohibited score/profile persistence. No network access was performed for this audit.

## Correction criteria

1. Keep the existing native flow identified as a component test harness. Do not use its successful stage entry as proof of original boot or game loading.
2. Recover the actual startup call graph, configuration reads, registration tables, error paths and shutdown order from the shipped loader/AVS/EA3/game artifacts. A new high-level boot enum with invented transitions is not equivalent.
3. Port original entry and scene-owner decisions with explicit service interfaces at OS/hardware boundaries. An unrecovered required service must report unavailability, not return a fabricated success or bypass its state transition.
4. Recover TEST dispatch, operator input mapping, page/control routines and their setting/hardware effects before presenting a TEST menu. Application settings are not a substitute.
5. Supply recovered configuration/profile/mode values to the already verified numeric modules. Preserve the original branches even when a selected offline mode does not exercise them.
6. Validate migrated routines against original instructions and integrate observed call/state traces at subsystem boundaries. Document the stubbed environment and unknown coverage in each comparison.
7. Retain original resource bytes and explicit unsupported behavior. Full AFP/page-controller recovery must replace the fixed-frame artwork policy before claiming complete original menus.

This audit changes documentation only. It does not certify new bootstrap code, remove the existing harness or complete AVS/TEST recovery.
