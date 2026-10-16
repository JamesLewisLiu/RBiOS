# Original CPU decisions

CPUState and OriginalRandom restore the original CPU logic using explicit native types. Addresses below are original PE virtual addresses at base 0x10000000; the source DLL hash is fixed in [MIGRATION](MIGRATION.md).

evaluate returns an optional CPUJudgment containing the original numeric judgment and event 11 or 12. The caller preserves the original dispatcher mapping at 0x1016B190 rather than treating event 11 as an already normalized scoring reason.

## Fields and ownership

| Native field | Original evidence |
| --- | --- |
| entries.size() | CPU +8 |
| lastIndex | CPU +20 |
| specialEnabled | CPU +24 |
| difficulty | CPU +12 -> profile +20 |
| meter | CPU +4 -> score +100 |
| entries[].judgment / tick | Arrays at CPU +52 / +56 |
| entries[].consumed / reserved | Byte arrays at +60 / +64 |
| specialOverride | CPU +68 |
| CPUNoteInput.index | note +160 -> chart +324 |
| kind / flags | chart +288 / +300 |
| relativeTick | note +192 |
| segmentElapsed / index / duration | note +360 / +364 / +368 |

Constructor 0x1015D8E0 sets lastIndex=-1, specialEnabled=true and specialOverride=-1. Profile difficulty and the current score meter are supplied explicitly. Original CPU reset does not silently reset lastIndex.

## Original functions

| Address | Decompilation line | Restored behavior |
| --- | --- | --- |
| 0x10065D20 | about 86194 | Four-word 32-bit CRandom seeding |
| 0x100658E0 | about 86072 | Original floating range |
| 0x100659D0 | about 86114 | percent > random(0,100) |
| 0x1015D8E0 | constructor | Original initial flags |
| 0x1015DA20 | about 292365 | Judgment distribution |
| 0x10005610 / 0x10005750 | about 26807 / 26884 | Original seeded shuffle |
| 0x1015DCB0 | about 292505 | Entry creation and shuffle; native vector replaces CRT storage |
| 0x1015D990 | about 292335 | Tick offsets, including original judgment-dependent +/-1 branches |
| 0x1015DE30 | about 292591 | Initialize distribution, shuffle and offsets; retain lastIndex |
| 0x1015D800 | about 292314 | Reserve matching entries after lastIndex while honoring flags |
| 0x1015DB50 | about 292428 | Special event-12 gate |
| 0x1015DBC0 | about 292453 | Tick threshold, judgment lookup and event selection |

Original random state at 0x10371FD0 becomes OriginalRandom.words. All consumers share the same stream; no per-note reseeding is introduced. The 19-row float table at 0x1036C808 is retained. Distribution, shuffle and offset generation consume the stream in original order.

Kinds 1/4 produce event 11. Other kinds with judgment zero can invoke the special gate. specialEnabled, override, current meter, note flag 0x20 and probability control event 12. These numeric branches are preserved instead of assigning new gameplay meanings.

Gameplay synchronizes the current PlayerScore.justReflec before CPU evaluation, matching original 0x1015DE90 and score +100 access. StageSession supplies the prepared opposing chart count and explicit CPU level 0.

## Validation and limits

tools/verify_cpu.py passes 3,478 isolated original-instruction comparisons:

- 19 parameter rows and 56 RNG/range cases.
- 456 distributions, including counts 0/1/2/3/7/91/1000/4096.
- 285 initialization, seeded-shuffle and offset cases.
- 2,062 evaluations including state mutation and special gates.
- 300 reservation cases.

CPU host checks total 5,239. Original CRT allocation/vector growth is replaced by bounded fixture storage. These CPU comparisons use their original default x87 fixture environment and finite CPU-use ranges.

Full stage difficulty selection, including 0x1015D4C0, and the original process seed source are unfinished. iOS supplies arc4random entropy to the restored algorithms; this is a platform adaptation. Full-song deterministic audits are separate in [STAGE_SESSION](STAGE_SESSION.md). ARM64 CPU play and device timing remain unverified.
