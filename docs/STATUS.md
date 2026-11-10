# Current status

Updated 2026-11-11. Implementation, host validation, Apple builds and device play are separate claims. The complete migration is not finished.

| Area | Implemented or verified | Remaining work |
| --- | --- | --- |
| Scope | Historical plans preserved; score/profile persistence removed | Future account data will come from a server |
| Restored core | Chart preparation, destinations, routes, states, pool ordering, touch, CPU, judgment, scoring and stage initialization | Full original page state machines, account eligibility and some presentation side effects |
| Complete-song audit | 2,309 catalog charts x both sides x auto/no-contact: 9,236 runs passed | No full original-game trace or device replay comparison |
| Hold/Switch presentation | Original geometry, textures, hidden movie bindings and updates; 62,563,101 audited ticks | JR/collection effect consumers and device presentation |
| Original resources | Direct PLY/IFS/texture/SD9/2dx reading and memory decoding; all 86 large character cards verified complete | Original fonts, full AFP VM, dynamic page bindings, general masks and nonrectangular geometry |
| Score display | Original score/combo glyphs, score labels, normal-layout anchors and per-tick score counter; 120 drawing and 2,400 counter cases match original instructions | Animated layout and device validation of the new score labels |
| Stage frame | Original top/bottom panels, round/difficulty labels, player/orb graphics, rectangular mask and independent child clocks; 908 reference samples passed | Account text/portrait bindings, score widgets and device verification of the newly restored frame |
| AFP clocks and pools | Switch/Bigbang/background sampling, 37 descriptors, hidden playback, reuse and within-note cleanup order | Full exhaustion behavior and independent cross-note same-tick cleanup comparison |
| Menu and stage flow | Import -> original title art -> song list/search -> recovered difficulty rules -> C++ play/CPU -> result/replay page | Full original page controllers, dynamic art/font bindings and device validation of the new flow |
| Display and input | Fixed 768 x 1360, 50%-100% scaling, inverse mapping, timestamps and pause/background cancellation; simulator corner/line/nested-mask checks passed | Physical multi-touch measurements and original scene pixel parity |
| Audio | Music sample clock plus original default local hit/switch/bigbang one-shots, four-lane suppression, continuous Hold/Switch loop, original difficulty voices and predecoded SD9 buffers | Rival stream, remaining menu audio and device synchronization |
| Scores and profiles | Memory only; no database, history or upload queue | Persistence intentionally excluded |
| Networking | Original fields, selected callbacks, protocol/transport isolation, mocks and configuration | Full EA3 envelope, discovery/card/profile/score flow and URLSession bridge |
| Embedded server | Lifecycle interface; unavailable without source | Server-source migration |
| Windows build | Zig C++17 and 30 test programs passed | Not ARM64/iOS compilation evidence |
| macos-15 Actions | Xcode 16.4 arm64 archive, minimum-OS validation and 32 simulator tests passed at b3a5dc1, including interpolation and effect buffering | Simulator tests do not establish iPadOS 14.6 hardware behavior |
| M1 / iPadOS 14.6 | User reports installation and provides stage-completion screenshots with corrected field framing | New frame graphics, performance, synchronization and instrumented touch testing |
| Live server | No probe, login or upload | Offline tests do not establish live compatibility |

The DLL supplies a version check and original catalog constants. It is not loaded as executable code. The numeric core is restored from original instructions and decompilation. The menu flow combines original title/selection/result artwork and recovered difficulty rules with native controls. Static AFP page samples do not constitute recovered original scene controllers, dynamic bindings or complete menu animation.

Resources remain unchanged. Decoded pixels, PCM and AFP state live in memory. Settings cover display size, resource access and future server configuration only.

The frame implementation passed [Actions run 42052166298](https://github.com/JamesLewisLiu/RBiOS/actions/runs/42052166298). Its unsigned IPA was inspected as arm64 device code with minimum OS 14.0. The new frame still requires a device check. Character preview cut edges are present in the original source image; see [CHARACTER_RESOURCES](CHARACTER_RESOURCES.md).

See [VALIDATION](VALIDATION.md), [STAGE_SESSION](STAGE_SESSION.md), [STAGE_LONG_PRESENTATION](STAGE_LONG_PRESENTATION.md) and [GAME_PRESENTATION](GAME_PRESENTATION.md) for evidence and boundaries.

The user reported the score/combo build passed their device check on 2026-11-10. This is user-reported acceptance, not instrumented timing or pixel parity. Packaging now produces matching IPA/TIPA files for TrollStore sharing. The supplied future server is recorded in [PROTOCOL](PROTOCOL.md); live integration remains pending.

The sound/rendering increment restores the original local sound selector (16,800 original-instruction comparisons), asks capable displays for up to 120 Hz, and interpolates stable note positions while retaining 60 Hz simulation. Supported stage textures and effects are prepared before play; intermediate input segments no longer construct redundant render snapshots. These changes target source-level first-use and allocation overhead, not a measured guarantee of frame rate. Interpolation adds up to 16.7 ms of visual delay. New device audio, frame-time and alignment checks remain pending. See [STAGE_SOUND](STAGE_SOUND.md) and [RENDER_TIMING](RENDER_TIMING.md).

The user reported the sound/ProMotion build is playable with occasional acceptable stutter on 2026-11-11. This is not a measured 120 fps claim. The menu increment retains that stage timing, releases inactive menu artwork, and renders static menu pages only when changed. Original resources are never converted. The supplied server remains unused; account unlock restrictions are not assumed to be recovered.
