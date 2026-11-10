# Original execution and migration map

The analyzed game is modules/reflecbeat.dll, PE32/x86 at base 0x10000000:

~~~text
MBR:J:A:A:2019121100
SHA-256 ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8
~~~

The existing modules - Copy/reflecbeat.dll.c is Hex-Rays 8.3 output, not compilable original C++. During recovery the local REA connection lacked a suitable native decompilation provider. Evidence therefore uses existing decompilation, original PE bytes, Capstone and bounded Unicorn execution. No REA evidence IDs, provider configuration changes or full-process traces are fabricated.

## Windows startup

~~~text
spice.exe -modules modules -smartea
~~~

This selects the module directory and Spice Smart EA behavior. The game also depends on AVS/EA3, AFP, Direct3D9, DirectSound/ACM and device APIs; it is not a standalone library with only two independent entry functions.

Exports are dll_entry_init at 0x10168840 and dll_entry_main at 0x10168AA0. Imports include libavs-win32.dll, libavs-win32-ea3.dll, libafp-win32.dll, libafputils-win32.dll, D3D9, DSOUND, MSACM32, ACIO and Windows system libraries.

A supplied historical log dated 2026-09-01 records AVS/EA3 2.15.8.0, prop/avs-config.xml, loading the game at 0x10000000, init and EA3 boot. It is previous Windows evidence, not a game launch during this migration or iPad DLL execution. No original server was contacted and no other Spice source directory was modified.

The selected approach restores native C++ and replaces platform dependencies. x86 pointers and ABI layouts are never used directly as ARM64 objects.

## Address map

| Original address / source | Native implementation | Evidence and scope |
| --- | --- | --- |
| 0x1014B870, decompilation about 276840 | Chart::parse | Original variable records; 2,315 charts parsed |
| 0x1014A230, 49E90/4B270/49BB0/4AD20/49670 | MusicSheet | Preparation, links, counts, destinations, directions and ordering;12 complete-field comparisons plus all charts; default option only |
| 0x10149AA0/497D0/49390 | Chart::additionalTicks | 15 hold/32 Switch counting comparisons; counts are not final score |
| 0x10065D20 | OriginalRandom | Original shared seed/stream; process entropy source separate |
| 0x1016ACA0; tables 102385B0/102385F0 | timingWindows | Original bytes and mode fallback |
| 0x1016AC10 | classifyTiming | 560 boundaries/modes/flags |
| 0x10158640/58700/587C0 | NoteTiming | Hold tick, Switch boundary and expiry;424 cases including 32-bit wrap |
| 0x101B4198,945 records x556 bytes | readCatalog | Original mapping and CP932 bytes; fixed-hash bridge |
| 0x10187C30, about 319712 | xrpcModules | Original services/cardmng/local2/lobby2 registration; not complete business networking |
| 0x100A3E40/3EA0/42D0,1015A240 | Motion | Segment velocity, integration and field geometry;1,090 comparisons |
| 0x10061520/61590/61CB0,10158BA0/59610/59820 | Touch | Original 15-slot queries and judgment geometry;3,690 comparisons |
| 0x1015D800..1015DE30 | CPU | Distributions, shuffle, offsets and gates;3,478 comparisons |
| 0x1016AC80..1016B940 | JudgeDispatch | Original input/CPU/auto/replay numeric branches;10,000 comparisons |
| 0x10159E10/1015F8C0/1015F6E0 | Gameplay | State submission, reason filtering and final bonus;1,500 comparisons |
| 0x10154660..10155260,10164160..101647C0 | Score | Combo, gauge, life, rates and results;2,007 comparisons |
| 0x100A3540/3380/3EF0,1015AAD0/A970/B060 | Route | Configuration, construction and JR redirection;8,600 comparisons using original D3DX scalar code |
| 0x1015A5E0 and related phases,1015AC00 | NoteFrame | 4,500 complete 448-byte states; external hooks remain explicit fixtures |
| 0x100619D0/61E50/60FD0/61810/61770 | TouchInput | Contact association/recycling;3,000 frames; original hardware filtering excluded |
| 0x101554C0/1007FFD0 | StageScoreInitialization | Original defaults and full 172-byte initializer;600 comparisons |
| 0x10157900,100302D0/30240,1015A100 | StageOrder | 1,000 complete sort identities and 1,000 angle bit patterns |
| 0x1015FC60/60620/E8C0/EE00/577D0 | StageSession | 60Hz order, creation, linked launch, judgment and release;9,236 host songs |
| 0x10155F10/55A00,10156690/56D40/57250,1015C2D0/C400 | Note/Long graphics | Original assets and geometry;420 long-draw comparisons plus full lifecycle audit |
| 0x1036BFD8,10066C30,10007050/7120/71F0 | OriginalAfpMoviePool | 37 original descriptors, hidden clocks and reuse;862 descriptor/pool and 6 clock checks |
| 0x101591D0/1015BDE0/10156690/10155890 | AFP cleanup | Original component/array release order;68 checks and 56 releases |
| 0x1009CEE0/C430/F1B0 and related functions | Protocol | Original read fields, card mapping and callback states; EA3 envelope/business parsing unfinished |

Each component guide supplies additional fields, source positions, fixture boundaries and reproduction commands. ChartObject models wire fields, not an x86 overlay. Timing arithmetic explicitly preserves original 32-bit wrap where required.

## Format and platform evidence

KBinXML/IFS/AVSLZ reading is checked against kbinxml/ifstools. Original PCM/MS ADPCM is compared with FFmpeg, trimming only padding outside fact frame count. Archive-wide audits and isolated function comparisons are different forms of evidence.

ViewportTransform, UIKit, Metal, SpriteKit, AVAudioEngine and NetworkTransport are native platform adaptations without invented original function addresses. Known method names include services.rb4get and player.rb6_player_read/write/read_score/start/end; names alone do not recover request schemas.

## Critical unfinished behavior

- Final JR/collection effect consumers, rival/menu sound paths and complete AFP side effects.
- Complete original title, selection and result scene controllers, font layout, dynamic bindings, full VM, masks and nonrectangular geometry; static menu artwork and difficulty switching are now migrated.
- Original full pool exhaustion behavior and independent same-tick cross-note cleanup ordering.
- EA3 envelope/negotiation, discovery/card handling, account lifecycle, complete read/write and score submission.
- Apple compilation, ARM64 floating-point behavior, real touch/audio synchronization, pixels and performance.

Local persistence is intentionally excluded. UIKit entry or mocked responses are never used to claim completion of missing original behavior.

The 2026-11-11 menu increment maps `0x1011A170`/`0x1011A380`/`0x100ADBD0` to [MenuSelection](MENU_SELECTION.md), `0x100C5EF0` and original AFP placements to [MenuPresentation](MENU_PRESENTATION.md), and the remaining held-note collection/lifecycle portions of `0x10160620`/`0x10163CE0` to [StageContinuousSound](STAGE_CONTINUOUS_SOUND.md). All fresh decompilation used the Official IDA MCP Server. Offline chart availability is an adapter policy, not recovered server entitlement behavior.
