# Original scoring and result calculations

Score.hpp/.cpp restores typed numeric state from original instructions and decompilation. The fixed DLL hash and PE base are recorded in [MIGRATION](MIGRATION.md). Offset comments identify evidence, not an ARM64 layout.

| Address / table | Restored behavior |
| --- | --- |
| 0x10154660 | Life update, including original 5000 boundary behavior |
| 0x101546C0 | Level/judgment/note-count life increment |
| 0x101548F0 | Battle score reset |
| 0x10154C80 | Gauge adjustment and optional floor at one |
| 0x10154D40 / 0x10284578 | Gauge/effect branches |
| 0x10155260 / 0x10292674 | Score submission, combo, counters and JR effects |
| 0x10164160 | Player reset preserving chart metadata and initialStreak |
| 0x10164260 | Consume JR meter |
| 0x101642B0 / 0x10164340 / 0x101643B0 | Projected, current and final achievement rate |
| 0x10164450 / 0x10164540 / 0x101647C0 | Performance score, clear type and medal |

Original judgment codes 0..4 and reason codes remain numeric. The original base-point table is {6,4,2,1,0,10,0,50}; reason filtering and note kind determine its use. Terminal combo bonus is handled through original 0x1015F6E0, not by adding 50 to every event. See [GAMEPLAY](GAMEPLAY.md).

BattleScore requires recovered chart/configuration input; it supplies no invented stage defaults. [STAGE_SCORE](STAGE_SCORE.md) records the separately recovered initializer. Native adapters reject invalid denominators, grades and player indices.

tools/verify_score.py passes 2,007 original-instruction comparisons: life increments 360, effects 108, event sequences 1,200, rates/results 300, reset 30 and JR consumption 9. Fourteen host checks pass.

The original note virtual +180 uses an explicit boolean fixture in this score suite; other score helpers execute original instructions. Nonfinite gauge values are excluded. These checks do not validate original result scenes, device audio/input or complete play.

Scores and results remain in session memory. No local score/profile/history database or deferred upload queue is implemented.
