# Original stage score initialization

StageScoreInitialization restores 0x101554C0, using defaults recovered from 0x1007FFD0. The default gauge and maximum arrays are {10000,10000,0,0,0}; the four scale defaults are one.

offline(sheet, catalogLevel) derives totals, long-tick counts at original chart+120, after-JR counts at +112 and selected chart side. Original catalog levels 1..15 become lifeLevel=level-1; other values become -1. This is explicit in-memory offline configuration, not a restored server profile.

The catalog DWORD at +464 is copied to original chart+84 as a timing option. It is not the stage end time. StageSession uses the minimum gate initialized to 1000 by 0x10149010, plus the chart-duration upper boundary described in [STAGE_SESSION](STAGE_SESSION.md).

tools/verify_stage_score.py passes 600 complete 172-byte comparisons with original instructions. The synthetic invalid-level fixture uses byte 0xff to represent -1. Two host initialization checks pass. No profile is saved and no server is contacted.
