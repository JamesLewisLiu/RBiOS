# Original touch-slot lifecycle

TouchInput restores logical coordinate snapshots into the original 15-slot contact pool. It does not sample hardware, launch the game, rewrite assets or persist scores.

Evidence: existing decompilation and original instructions, with the fixed DLL hash in [MIGRATION](MIGRATION.md).

| Address | Behavior |
| --- | --- |
| 0x10060E50 | Pool reset |
| 0x100619D0 | Clear transient state and recycle prior released slots |
| 0x10061E50 | Globally nearest point matching |
| 0x10060FD0 | Update existing contacts |
| 0x10061810 | Allocate new contacts |
| 0x10061770 | Suppress duplicate multi-touch contacts |

advance(points, config) accepts up to 25 already processed logical points and runs once per game tick. It cleans prior state, computes old-slot/new-point distances, repeatedly matches the globally shortest pair, marks unmatched old slots released, and fills first-free slots with new points. Ties retain slot/input order.

UITouch identities are not original contact IDs. Original IDs increment independently and wrap from INT32_MAX to zero. Even rejected contacts consume an ID when the pool is full.

New stateBits=1, continued=2 and released=4. A released slot stays active for one frame, then becomes inactive with stateBits=255 and ID=-1. selected/captured clear each frame; blocked survives until recycling. History retains initial/previous coordinates, stationary duration, contact duration, age, 20-pixel movement and processed flags through typed fields.

TouchContact.reuseDelay names original slot +60, but the algorithm treats it as a maximum reuse distance, not a countdown. Default -1 is unlimited. Ordinary first judgment sets 40, requiring subsequent matching distance strictly below 40; no per-frame decrement occurs. Global default matching distance is strictly below 2720, from 0x10372084. StageSession supplies original stage distance 70.

Original 0x100623A0 also invokes hardware sampling, transform 0x10061BB0, mode 0x100621E0 and near-line filtering 0x10061A60. iPad supplies inverse-mapped coordinates; those hardware-specific filters are not migrated. Duplicate suppression uses explicit configured bounds, default logical Y=80..1222.

advance does not invent supplemental coordinates. The supplemental-input adapter supplies them separately. reset clears contacts while preserving increasing IDs, matching original pool reset. UIKit snapshot order must remain stable because ties depend on original input order.

tools/verify_touch_input.py compares 3,000 consecutive frames against isolated original instructions at x87 CW=0x027f: up to 25 points, pool exhaustion, wraparound, contact/release, jitter, distance limits, selection clearing and duplicate suppression. Five host checks also pass. Report: .build/touch-input-parity.json. UIKit/device delivery is unverified.
