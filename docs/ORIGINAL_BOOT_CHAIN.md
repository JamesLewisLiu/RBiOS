# Original loader, AVS and game-entry recovery

Updated 2026-11-11. This document separates the host launcher from AVS, EA3 and game code. It is a static source/instruction analysis, not proof of a successful native boot. No server, game executable or NVRAM writer was run.

## Target identities

| Module | SHA-256 |
| --- | --- |
| reflecbeat.dll | ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8 |
| libavs-win32.dll | d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91 |
| libavs-win32-ea3.dll | 12341ec08d67fc62adec8098dd3f439553f3eb9a1998a41c1fbeaf10d1ec82ae |

Fresh game pseudocode was obtained through the Official IDA MCP Server. Original PE instructions independently establish the identity mutation described below. Relevant launcher source was copied into the E-drive analysis workspace before inspection; the Spice repository was not modified.

## Four separate layers

1. The supplied Spice launcher loads AVS core and EA3 modules, boots AVS, initializes its SSL adapter, copies default configuration files, then loads the game DLL and its exported entry points. These are host-loader operations, not code inside reflecbeat.dll.
2. Spice's AVS 2.15.8 binding maps core boot to `XCd229cc0000aa` at `libavs-win32.dll:0x100138C0`. Its wrapper reads `prop/avs-config.xml`, adjusts selected property nodes and allocates two heaps before calling the export. The wrapper is not the implementation of AVS services.
3. The EA3 wrapper prepares EA3/app configuration and the software ID, calls game `dll_entry_init`, applies launcher network policy and then calls `XE592acd00008c` at `libavs-win32-ea3.dll:0x1000AD20`. The `-smartea` wrapper can perform a server check and replace the service URL with a local server. This has not been executed during recovery and must not be mistaken for recovered game networking.
4. The host eventually invokes game `dll_entry_main`. The complete AVS and EA3 service implementations and native scene-owner lifecycle remain unported. A list of callbacks that always succeeds would not restore them.

## Game exports and initialization

`dll_entry_init` at `0x10168840` mutates the software identity, invokes `0x1005B580`, invokes NVRAM rebuilding at `0x100B2BD0`, then calls `0x101686D0` with the predicate `softwareId[3] == 'K'`. Its returned byte is one; that does not establish success of every called subsystem.

The NVRAM rebuild routine reads `/dev/nvram/ea3-config.xml` and tests `/ea3/pos/coin/mode_reflesia`; if the node is missing it copies `/prop/defaults/ea3-config.xml`. It separately reads `/dev/nvram/eacoin.xml`, tests `/opt/eacoin/game/nstart`, and may copy the corresponding default. `0x101686D0` reads and potentially rewrites the `apsmanager` property through AVS file/property APIs. These are cabinet/configuration semantics, distinct from the user's excluded score/profile persistence. No such writes are enabled by the current recovery.

`dll_entry_main` at `0x10168AA0` resolves the module handle and calls `0x101689E0`. The wrapper requests the Windows one-millisecond timer period, constructs the application object with `0x10169040`, conditionally initializes through `0x1016A600`, and only on success calls the main loop `0x10169D00` followed by teardown `0x10169A70`. It ends the timer request, clears a global and returns `1100 + application.error`.

`0x1016A600` runs an initial helper, waits 3000 ms, creates/configures the window and rendering device (`0x10168F20` and `0x101690F0`), binds the window property and module-directory state, and enters `0x1016A190`. These platform calls require explicit native adaptation; skipping their dependent state is not equivalent behavior.

## Actual game-boot sequence

| Original routine/call | Behavior and failure boundary |
| --- | --- |
| 100907A0, 1008BA70, 1008AF80, 1008A880, 1008B3C0 | Game-data initialization before the first boot progress message |
| AVS exports 109 then 12D | File-cache stage |
| AVS export 14D then 0CE | Mount `/e_drive` from `e:/`; nonpositive mount result sets application error 3 |
| Allocation and 1007DC80 | Network-system object initialization; null object sets error 4 |
| 10024490, 10024560, 100245D0 | BM2D setup with original allocation arguments 0x1400000 and 0x4800000; failure sets error 5 |
| 1005FB50, 100B2B50(0), 100B2B50(2) | Further graphics/resident-file initialization |
| 1006BFE0, 10169980, 100769B0 | System sound, taboo data and font initialization |
| Error-status virtual calls, 1006A670/10069010/1006A1A0 | Error-status and process-owner setup |
| QueryPerformanceFrequency, InitializeWinIo | Platform timer and cabinet-I/O setup |

The wrapper consequently returns 1103/1104/1105 for the three explicit failures above. This table records original ordering; it is not an implementation that synthesizes success. Individual subsystem effects, exception cleanup and remaining dependencies still need recovery.

The main loop is not just a call to the chart session. It pumps Windows messages, runs device/input and service updates, processes several callback-owner state machines, updates scene objects, advances BM2D with the original float `0.016676668`, updates presentation/device owners, evaluates shop-closing/coin-blocker state, runs AVS work, presents and updates sound managers. The existing direct audio-clock-to-StageSession harness does not restore this loop.

## Implemented bounded identity mutation

`OriginalEntryIdentity` implements only `0x10168840..0x10168983`, before the initialization side effects. The raw field bytes are modeled as owned data, never x86 pointers or an ARM64 ABI overlay.

- Model becomes `MBR` regardless of input model bytes.
- Destination changes only when input byte 3 is nonzero.
- The original spec/revision blocks compare one-character strings with lengths 3 and 10. Original instructions at `0x101688E1` and `0x10168919` confirm these impossible branches; they are preserved, not corrected into invented assignments.
- The ten date bytes replace the previous date only if none is zero.
- The return value of this helper is the Korean-region predicate for the later call, not a boot-success status.

The native test passes. `tools/verify_original_entry_identity.py` passes 512 differential cases against original x86 instructions, with randomized prior fields, destination variants and embedded-zero dates. Only `strcpy_s` and `memcpy_s` are modeled. Execution stops before the original initialization calls; no AVS startup, server operation, NVRAM write or full game boot is claimed. The helper is intentionally not wired into a fake successful startup path.

## Next dependencies

Recover AVS service boot and property/filesystem ownership, EA3 initialization and error policy, then game-data/graphics/audio/font/device/process initialization and scene dispatch. Connect original TEST entry through its actual input/scene owner. Keep existing numeric components as regression targets, and compare integrated behavior only after these owners and state transitions are present. Missing behavior must remain explicit; replacement UIKit menus, frozen AFP artwork and offline policy defaults do not satisfy this requirement.

## AVS2 service bootstrap inspected directly

Fresh Official IDA MCP analysis of the original AVS library identifies `XCd229cc0000aa` at `0x100138C0` as AVS2 2.15.8 r6631 boot. It initializes logging, checks both supplied heaps, imports/validates the 2,528-byte configuration (`0x10013A70`), applies product-mode corrections (`0x10013B30`), then initializes descriptor/thread/filesystem/private-network allocation (`0x10013BC0`), standard heap/stdio (`0x10013D50`), time (`0x10013DC0`), filesystem registration and mounts (`0x10013E00`), a platform stage (`0x100140B0`), networking (`0x10014140`), SNTP (`0x100142D0`) and optional netsci filesystem (`0x10014310`). The final calls and boot-complete byte are not replaced with fabricated success.

Hex-Rays failed on `0x100140B0`; its original instructions were retained for follow-up rather than omitting the stage or inventing pseudocode. Custom register arguments in other helpers require instruction-level validation before implementation. Only the product-mode configuration transformation is implemented so far; see [AVS_BOOT_CONFIG](AVS_BOOT_CONFIG.md).

Fresh Official IDA analysis of original EA3 `0x1000AD20` separately shows finalizer creation, configuration/service initialization, facility initialization, conditional cardmng/package/userdata/userid registration, paired cleanup callbacks, start and finalizer fixation. Fatal failure paths and cleanup ownership are real dependencies, not mock responses. This is static recovery evidence, not an executed network boot. No server or SNTP endpoint was contacted.
