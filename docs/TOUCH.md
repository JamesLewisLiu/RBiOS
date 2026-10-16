# Original touch judgment queries

Typed TouchState and NoteTouchState preserve original fields without copying x86 object layouts. Source addresses refer to the fixed DLL and existing decompilation in [MIGRATION](MIGRATION.md).

| Address | Decompilation line | Interface | Scope |
| --- | --- | --- | --- |
| 0x10061520 | 83111 | supplementary | Original supplemental query |
| 0x10061590 / 0x10061630 | 83145 / 83191 | held | 15 slots, stateBits&2, original coordinate/ID updates |
| 0x10061700 | 83259 | find | Active flag and ID lookup |
| 0x10061CB0 / 0x10061DF0 | 83628 / 83702 | press | stateBits&1 and original selection rules |
| 0x100A4F80 | 129760 | fieldContains | Directional geometry and FieldGeometry values |
| 0x101584B0 | 288107 | inField | Original virtual +172; touchScale and 36-pixel geometry |
| 0x10158550 | 288114 | missed | Original miss geometry |
| 0x101586B0 | 288198 | extendedWindow | Wide-note and original field bounds |
| 0x10158750 | 288228 | touchWindow | Lifecycle, cooldown and type-dependent timing |
| 0x10158BA0 | 288480 | maintainHold | Hold region, optional supplemental contact and five-step grace |
| 0x10159610 | 289092 | switchPress | Segment press/release, reasons 5/7 and contact recording |
| 0x10159820 | 289162 | firstPress | Width/lower-field handling and original reuse distance 40 |
| 0x10159D10 | 289400 | maintainSwitch | Reverse-segment half-width, release and hold transition |

The original container +212 points to 15 records with 64-byte stride and data beginning at +4. Active state, ID, coordinates, state bits, selected, captured, blocked and reuseDelay become named fields. lastSelectedId replaces global 0x12479018. touchScale replaces global 0x1036BF0C; it controls judgment geometry independently of display scaling.

missed accepts the restored inField geometry callback and throws if a required implementation is absent. extendedWindow's default bottom 1222 comes from 0x100A4B90(0). This suite compares the original default field and original 768 x 1360 logical coordinates.

[TOUCH_INPUT](TOUCH_INPUT.md) restores per-frame association, recycling and selection clearing. UIKit timestamps, viewport changes and pause/background cancellation are connected in code. Original hardware filtering and actual Apple input delivery remain unverified.

## Validation

TouchTests has 25 checks. tools/verify_touch.py passes 3,690 isolated original comparisons, covering scanning, first press, holds/reverse holds, Switch, miss and windows, including state mutations. The M group supplies a boolean miss-boundary fixture. G adds 1,512 cases executing original +172 and four directional containment queries; J adds 300 cases reconnecting actual original +172 to the complete miss query.

Report: .build/touch-parity.json. These checks execute no game process, Windows API or server connection and do not establish UIKit/device correctness.
