# Original per-tick note state

Evidence is the existing decompilation and fixed DLL hash in [MIGRATION](MIGRATION.md). This is restored native C++, not Windows DLL execution. It persists no results and converts no resources.

## Initialization and scheduling

initializeNote restores 0x10159090 using original stage time and prepared start/travel ticks. It retains late-path handling and the minimum 10-tick branch. Chain pointers become explicit object-ID lookups. For kind 4, durationTicks must follow 0x101493D0's original millisecond difference; summing individually truncated segment ticks is not equivalent.

selectInitialState restores 0x1015B720: flag 0x10 selects Top entry, kind 1 holdRoute, kind 4 switchEntry, otherwise normal.

advanceNoteState restores scheduler 0x10008D60. It commits a pending phase first, recording the old phase and clearing stateFrame, then a pending state and stateTimer. It executes the current phase and increments both counters. Requested transitions take effect on the next call, without immediate recursive entry.

| Address | Phase | Restored behavior |
| --- | --- | --- |
| 1015B3F0 | normal | Route, extra first-frame integration, miss fade and chain break at special reflection |
| 1015B250 | holdRoute | Initial hold route, then holdEntry |
| 1015B0D0 | switchEntry | Initial Switch route, then switchTravel |
| 1015A5E0 | holdEntry | Tail entry using truncated 380 / speed |
| 1015A540 | holdTravel | Fixed tail offset; requests exit at relative tick zero |
| 10159B30 | exit | Tail shortening, held state and presentation requests |
| 1015A470 | switchTravel | Integration; original judgment-line crossing requests longTail |
| 1015A1A0 | longTail | Segment timer, final timeout, completion and fade |
| 10159AF0 | release | Alpha decreases by original 0.15000001; releases contact at zero |
| 1015A740 | missFade | Integration and missed-note fade |
| 1015B590 | topEntry | Current original BPM controls entry, followed by timeline rebuild |

NoteFrameContext supplies route kind, reflection count, direction, current BPM and dynamic route callbacks. A required missing builder throws. Top BPM comes from the stage query, not a guessed constant in the adapter. Position and timeline remain owned by the same note.

Frame-end follows 0x1015AC00: update presentation, advance the motion timeline unless phase is exit, then finishNoteFrame. It decrements relative tick, increments elapsed, clears inputBlocked, decrements cooldown and returns the original 0x101587C0 deletion condition. Inactive notes do not advance. The caller owns display-object release.

## Validation boundary

tools/verify_frame.py compares 4,500 complete 448-byte states with isolated original execution through rbnoteframe --batch. Coverage includes initialization, tail/exit/fade, frame-end, initial selection and the actual scheduler running normal/hold/Switch phases. Scratch pointer identities are never copied into ARM64 objects.

Original held-presentation 101589B0 is a counted fixture. Frame-end motion/deletion hooks are explicit fixtures. State comparisons execute original integration and timeline code on ready routes; rotation/audio virtuals are stubs. Top-entry BPM is fixed at 120 and timeline rebuild is a fixture, while transition/ramp instructions execute. Remote reception, special rebuilding, full AFP lifecycle and device audio synchronization are separate evidence.
