# Original difficulty window

`MenuSelection` recovers the selection portion of `MusicSelect2Component::CNoteGradeSelectWindow`. Fresh Official IDA MCP pseudocode for the four routines below was reviewed against saved decompilation and original x86 instructions on 2026-11-11.

| Address | Restored behavior |
| --- | --- |
| `0x1011A170` | Direct lower panels, shared upper-panel toggle, next-available cycle, confirmation, detail toggle and five-tick repeat gate |
| `0x1011A380` | Enable mask relative to initial difficulty and account eligibility |
| `0x100ADBD0` | Same-audio-group test using four catalog suffix bytes at entry +549 |
| `0x100AE270` | Inspected only: eligibility depends on chart presence, account restrictions and per-chart unlock status 1 or 2 |

Panels 0 and 1 directly select their enabled slot. Either upper panel selects slot 2 from a lower slot when possible, otherwise slot 3. Once on slot 2 or 3, either upper panel toggles to the other enabled upper slot. The next control cycles forward through four slots, skipping disabled slots. If every slot is disabled, one complete cycle returns to the original selection. An accepted next input sets cooldown to five, then the same tick decrements it to four. With no panel selected, confirmation is tested only when the next branch was not taken. Detail toggling is independent.

The original enable mask is `(candidate <= initial || eligible(candidate)) && sameAudio(candidate, initial)`. The all-space audio suffix case permits all audio-group comparisons; otherwise exact suffix-byte equality is required. This mask belongs to switching within an already-open difficulty window. It must not hide unrelated chart choices throughout the entire song catalog.

The account eligibility routine is not replaced with an always-successful recovery. The native API receives eligibility as an explicit input. Any offline availability policy is a platform-adapter decision, separate from original account and unlock behavior. Invalid difficulty indices are rejected by the native boundary instead of reproducing unsafe original memory access.

## Validation

`MenuSelectionTests` checks selection, masks, debounce, confirmation, detail state, audio grouping and invalid indices. `tools/verify_menu_selection.py` passed 24,576 comparisons against the isolated original `0x1011A170` instructions, covering every four-bit mask, all four current difficulties, six panel values, four cooldown values and every combination of next, confirm, detail and debounce flags. UI input polling, visual refresh, sound playback and telemetry callees are stubbed; selection/state/cooldown instructions execute unchanged. Account eligibility and live UI timing are not established by this oracle.

Target DLL SHA-256: `ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8`.

This module does not constitute a complete original menu migration. Title and result scene transitions, sorting/filtering, account restrictions, networking and AFP page scripts require separate recovery. Original assets remain unchanged; this code performs no filesystem writes or network access.

## Difficulty announcement audio

After changing selection, `0x1011A170` indexes the four DWORDs at `0x10292624` and calls `0x1006BC80(id, 50)`. The original PE table contains `[16272, 16352, 16432, 16512]`. Their SD9 header indices independently resolve to `data/sound/sys_sd_rb6/sys_vox_basic.sd9`, `sys_vox_medium.sd9`, `sys_vox_hard.sd9`, and `sys_vox_whitehard.sd9`, respectively. `difficultyMenuSound` exposes these exact resources. Trigger it only when the result reports `changed`; blocked or repeated selections do not announce. The argument 50 is the original playback argument and is not a normalized gain. The oracle verifies all four table values and SD9 identities without converting the resources.

## iPad integration

The imported chart list supplies the explicit offline availability mask; no server entitlement is inferred. The native selector uses original lower-panel and upper-toggle behavior, next-available cycling, confirmation and detail state. Elapsed empty 60 Hz ticks drain the original cooldown; the main-thread button adapter supplies discrete actions. Difficulty voice playback uses the recovered selection-change table and stops when leaving the page or backgrounding. Original files decode into RAM only.

The title/song-list/difficulty/result navigation and search control are native platform flow. They do not recover the original complete page state machines. Results are read only from the completed in-memory session; replay creates a fresh session with the same chart. No result is saved or queued for upload.
