# Stage judgment sound migration

The default profile's local judgment sound selection is restored from original
x86 code. The app reads the original SD9 files and decodes PCM into memory before
stage playback. No replacement sounds or converted resource files are shipped.

## Evidence

Target `modules/reflecbeat.dll` SHA-256:
`ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8`.

Fresh pseudocode for `0x1015E620`, `0x10158910`, and `0x10163A10` was obtained
through the Official IDA MCP Server. Saved original decompilation and original
instructions establish the surrounding calls and resource identifiers.

| Original address | Recovered behavior |
| --- | --- |
| `0x1015F8C0` | Apply note virtual `+144`, then invoke stage sound dispatch `+68`. |
| `0x1015E620` | Forward reasons 0, 1, 2, 3, 5, 6, 11, 12 to note virtual `+152`. |
| `0x10158910` | Accept only local side 0 and grades 0 through 4; pass note `+372`. |
| `0x10163A10` | Select grade, bigbang, and alternating switch sounds; queue arbitration. |
| `0x10163960` | Initialize the selected profile bank and optional miss sound. |
| `0x100B0790`, `0x100B0820`, `0x100B08B0` | Default profile good, great, and just IDs. |
| `0x1015FC60` | Clear pending lanes and increment cooldowns once per simulation tick. |
| `0x10163CE0` | Play pending lanes and reset their cooldowns. |
| `0x1006BC80` | Convert sound ID into `(id - 8192) / 80` bank index and sample slot. |

The SD9 unsigned 16-bit header field at byte 30 independently confirms each bank
index. The selected files contain stereo, 16-bit, 44,100 Hz PCM RIFF payloads.

## Selection and scheduling

The current offline profile uses the original default sound bank. Grades 0 and 3
select `just`, grade 1 selects `great`, and grade 2 selects `good`. Misses remain
silent unless the original miss-sound option is explicitly enabled. Reason 6
promotes a non-miss grade to 0. Kind 3 selects `bigbang` for any non-miss grade.
For other kinds, note `+372` equal to 0 or 1 selects `long_on` or `long_end`.
The integration must pass the switch direction after the note event has applied.

Ordinary, bigbang, long-on, and long-end requests occupy four independent lanes.
Each lane requires more than two simulation ticks since its previous playback.
The first ordinary request wins within one tick, except that an enabled damage
sound supersedes it and bypasses the cooldown. Display refresh rate does not
affect these rules. Every 60 Hz simulation tick drains its requests; presentation
can accumulate them when catching up several simulation ticks at once.

| ID | Original file under `data/sound/sys_sd_rb6/` |
| --- | --- |
| 10192 | `sys_shot_default1_just.sd9` |
| 11312 | `sys_shot_default1_great.sd9` |
| 12432 | `sys_shot_default1_good.sd9` |
| 14752 | `sys_shot_default1_long_end.sd9` |
| 14832 | `sys_shot_default1_long_on.sd9` |
| 14912 | `sys_shot_default1_bigbang.sd9` |
| 15072 | `sys_shot_damage.sd9` (optional; silent by default) |

The original playback argument 50 is not a gain: the SD9 branch of
`0x1006BC80` ignores that argument. The platform adapter uses an explicit unity
gain and centered pan. Original volume configuration and all other profile banks
are not exposed by this migration.

## Verification and remaining work

`StageSoundTests` covers grade selection, event suppression, switch direction,
independent lanes, first-request arbitration, damage priority, cooldown edges,
and reset/replay behavior. `tools/verify_stage_sound.py` compares the native
selector with isolated original instructions executing the complete
`0x1015E620 -> 0x10158910 -> 0x10163A10` chain and checks original SD9 identities.
The original executable is never launched and the test makes no network calls.

CPU judgment callbacks deliberately do not synthesize rival hit sounds: the
original game uses the separate `0x1015E7C0 -> 0x10149230` stream for those sounds.
That stream remains unported. The held-note loop (`14672`,
`sys_shot_default1_long_loop.sd9`) is selected separately by `0x10160620` and
started/stopped by `0x10163CE0`; its restored lifecycle is documented in
[STAGE_CONTINUOUS_SOUND](STAGE_CONTINUOUS_SOUND.md). Menu audio, the other profile banks, and exact hardware
mixing/loudness remain outside this increment. Passing offline code tests does
not establish device audible output or end-to-end touch-to-sound latency.
