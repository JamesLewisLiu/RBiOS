# Current status

Updated 2026-11-05. Implementation, host validation, Apple builds and device play are separate claims. The complete migration is not finished.

| Area | Implemented or verified | Remaining work |
| --- | --- | --- |
| Scope | Historical plans preserved; score/profile persistence removed | Future account data will come from a server |
| Restored core | Chart preparation, destinations, routes, states, pool ordering, touch, CPU, judgment, scoring and stage initialization | Full original page state machines and some presentation side effects |
| Complete-song audit | 2,309 catalog charts x both sides x auto/no-contact: 9,236 runs passed | No full original-game trace or device replay comparison |
| Hold/Switch presentation | Original geometry, textures, hidden movie bindings and updates; 62,563,101 audited ticks | JR/collection effect consumers and device presentation |
| Original resources | Direct PLY/IFS/texture/SD9/2dx reading and memory decoding | Original fonts, full AFP VM, dynamic page bindings, masks and nonrectangular geometry |
| AFP clocks and pools | Switch/Bigbang/background sampling, 37 descriptors, hidden playback, reuse and within-note cleanup order | Full exhaustion behavior and independent cross-note same-tick cleanup comparison |
| iPad stage entry | Import -> difficulty -> C++ play/CPU -> session results connected in code | Apple compilation and actual entry unverified |
| Display and input | Fixed 768 x 1360, 50%-100% scaling, inverse mapping, timestamps and pause/background cancellation | Apple XCTest and physical multi-touch validation |
| Audio | Block decoding, sample clock, EOF/interruption/resume connected | Original hit sounds and device synchronization |
| Scores and profiles | Memory only; no database, history or upload queue | Persistence intentionally excluded |
| Networking | Original fields, selected callbacks, protocol/transport isolation, mocks and configuration | Full EA3 envelope, discovery/card/profile/score flow and URLSession bridge |
| Embedded server | Lifecycle interface; unavailable without source | Server-source migration |
| Windows build | Zig C++17 and 22 test programs passed | Not ARM64/iOS compilation evidence |
| macos-15 Actions | Xcode 16.4 archive, minimum-OS validation and simulator workflow prepared | Awaiting user push; no Actions build has run |
| M1 / iPadOS 14.6 | Not run | Installation, entry, pixels, performance, synchronization and full play |
| Live server | No probe, login or upload | Offline tests do not establish live compatibility |

The DLL supplies a version check and original catalog constants. It is not loaded as executable code. The numeric core is restored from original instructions and decompilation. UIKit menus and result dialogs currently provide the platform entry; they are not recovered original title/selection/result scenes.

Resources remain unchanged. Decoded pixels, PCM and AFP state live in memory. Settings cover display size, resource access and future server configuration only.

The project is being committed locally for a user-managed push. Actions will trigger on that push; monitoring requires the resulting repository/run URL.

See [VALIDATION](VALIDATION.md), [STAGE_SESSION](STAGE_SESSION.md), [STAGE_LONG_PRESENTATION](STAGE_LONG_PRESENTATION.md) and [GAME_PRESENTATION](GAME_PRESENTATION.md) for evidence and boundaries.
