# Continuous Hold and Switch audio

The original stage uses one shared continuous loop for local held Hold (kind 1) and Switch (kind 4) notes. The offline default path is restored by `StageContinuousSound`; overlapping held notes do not start multiple voices or restart the loop.

## Original mapping

Fresh Official IDA MCP pseudocode and original instructions for the version-bound DLL establish these rules:

| Address | Behavior |
| --- | --- |
| `0x1015FC60` | Clear pending loop slot +96 at each simulation tick, retaining current slot +100. |
| `0x10160620` | After note evaluation, collect local side 0, kinds 1/4 with held byte +309 and enabled sound manager +56. Select loop ID 14672. |
| `0x10163CE0` | Stop when no loop is requested; start when requested differs from current; otherwise retain playback. |
| `0x1016B940` | Update held state during judgment evaluation, before event application. |

The observer runs immediately after each actor is evaluated, even when no judgment event is emitted. Deriving this solely from presentation notes would be incorrect: a final evaluated actor can still request the loop before its release. The original event application does not modify the held byte. Each original 60 Hz step begins and ends aggregation; 120 Hz drawing does not change it.

The resource is `data/sound/sys_sd_rb6/sys_shot_default1_long_loop.sd9`, ID 14672. Its original SD9 index at byte 30 independently confirms the ID. It contains stereo 44,100 Hz PCM. The iOS adapter decodes it in memory and uses a looping player node. No WAV or replacement asset is written.

## Validation and limits

153 native checks cover side, kind, held/enabled flags, overlapping notes, state retention, stop/restart and reset. `verify_continuous_sound.py` compares all four default-bank current/desired state combinations with the original `0x10163CE0` instructions, stubbing only hardware start/stop calls. It also checks the loop SD9 identity. These checks do not establish device audibility or exact loop seam/mixer parity.

The migration covers normal offline gameplay. Original attract-mode mute gating, other loop sources, rival sound streams and hardware mixer configuration are not included. Pause/background silence and resumption are explicit platform lifecycle adaptations; they do not emulate cabinet suspension. Device tests must confirm release, overlapping holds, Switch transitions, pause/resume and stage exit all stop or retain the loop as intended.
