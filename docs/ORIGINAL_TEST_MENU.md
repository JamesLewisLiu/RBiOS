# Original TEST main-menu recovery

This module restores the original main TEST menu table and its decision logic. It is not an iPad settings screen or a replacement TEST implementation. It is not connected to the current harness navigation, and it does not claim that any TEST subscene is functional.

## Evidence

Target: `modules/reflecbeat.dll`, SHA-256 `ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8`.

Fresh Official IDA MCP pseudocode for `0x100D5600`, `0x100D5AF0`, `0x100DD7C0`, `0x100D5710` and `0x100D5780` is saved under the local analysis tree in `original-boot-review/official-boot-test-pseudocode.json`. Original instructions, rather than decompiler loop-bound guesses, validate the migrated decisions.

| Address | Behavior | Recovery status |
| --- | --- | --- |
| `0x100D5AF0` | MAIN MENU initialization; choose region table, count records until scene -8; reset selection; initialize EA3, input/audio and exit state | Tables/count/initial selection identified; service and scene initialization remain unported |
| `0x103671E8` | Region 0 table, 18 records | Scene IDs, labels and initial flags matched to PE bytes |
| `0x103672D8` | Regions 1, 2, 3 table, 16 records | Scene IDs, labels and initial flags matched to PE bytes |
| `0x100D5600` | Confirm, navigation, subscene/exit dispatch decisions | Restored and compared against original instructions |
| `0x100D5780` | Update EVENT MODE / GAME MODE disabled flags from original external state | Restored and compared against original instructions |
| `0x100D5710` | Wait for EA3-derived entry state before activating the main update | Inspected only; no successful readiness is fabricated |
| `0x100DD7C0` | Allocate original UI buttons and reset pressed input | Inspected only; original UI/hardware input binding remains unported |

## Exact decision behavior

The input globals are `0x12477D3C` (pressed) and `0x12477D44` (repeat). Confirm has priority when pressed mask `0x30` is nonzero. Ordinary rows request their original scene ID; scene -5 requests exit, and scene -4 additionally writes the original event-mode request. The returned decision is an obligation for the real scene manager, not a simulated successful dispatch.

Repeat mask `0x42` moves down and takes priority over repeat bit 1, which moves up. The scan wraps and skips rows whose scene is `INT32_MIN` or whose flags contain either low bit. Confirmation itself does not add a new disabled-row check: the original function dispatches the currently selected row. The native boundary rejects invalid indexes and tables with no selectable rows, rather than reproducing unsafe indexing or an infinite scan.

`updateOriginalTestExitFlags` changes only flag bit 1. GAME MODE is disabled when any of the three original bytes at `0x12477CCC` or byte `0x12476DFA` is nonzero. EVENT MODE is enabled only when none of those blockers is present and the word at original system object +60 equals 1. The adapter must supply those recovered states; they must not become arbitrary offline defaults.

Region 0 includes ECOMODE OPTIONS (scene 54) and VIRTUAL COIN (scene 55); the other table omits them. Both tables retain original I/O, screen, color, ROM, card, sound, game, coin, network, bookkeeping, clock, factory-settings and system-information destinations. Listing factory settings or network options does not execute them. No hardware operation, network call, settings write or reset is performed by this module or its tests.

## Verification

`OriginalTestMenuTests` checks wraparound, skipped rows, repeat priority, confirm priority, normal scene IDs, exit/event mode, disabled flags and invalid native inputs. `verify_original_test_menu.py` executes unchanged `0x100D5600` and `0x100D5780` instructions for 6,120 combinations, compares every native decision and flag result, and independently compares both full native record tables to the original PE bytes. Only virtual scene/exit/sound dispatch calls are intercepted and recorded. No target process or service is launched.

The TEST initializer, EA3 entry gate, complete subscene controllers, original input acquisition, renderer, configuration storage and return-to-game lifecycle remain required. These recovered functions cannot replace those missing dependencies.
