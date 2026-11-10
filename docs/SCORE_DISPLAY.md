# Original displayed-score clock

`updateScoreDisplay(BattleScore&)` restores original `0x10155460` from the fixed REFLEC BEAT DLL SHA-256 `ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8`. It changes only the displayed scores at original offsets +8/+12 and animation flags at +16/+17. Actual scores remain at +0/+4.

For each side, a clear animation flag copies the actual total immediately. A set flag advances the displayed total by one toward the actual total on each update. Equality clears the flag. There is no invented percentage interpolation or elapsed-time multiplier.

The original manager update `0x10164FC0` invokes the two player updates and then `0x10155460`. Its virtual slot is `0x1028D78C`, offset +8 in the `CStageInfo` vtable. The normal stage loop `0x1015FC60` updates both actor arrays and the stage's judgment callbacks, invokes this manager slot, then updates presentation components. The native session therefore advances the displayed score once after each logic step and before delivering its presentation snapshot. This preserves updates when several 60 Hz ticks are processed in one audio-clock advance.

## Evidence and validation

The saved decompilation at `modules - Copy/reflecbeat.dll.c` supplies the control-flow context. Original PE instructions `0x10155460` through `0x101554BE` confirm the signed comparison, one-point movement, flag clearing and two-side loop. The normal-loop call sequence was also checked in the original instruction bytes. These are static and isolated-instruction observations; no live game process or server is involved.

`tests/ScoreDisplayTests.cpp` checks immediate copying, upward/downward animation, completion, equality and signed boundaries. `tools/verify_score_display.py` executes the actual original function in an isolated x86 fixture and compares the restored native executable. All **2,400 cases** pass, including boundary combinations and deterministic random signed totals. The fixture verifies that all other bytes in the 172-byte original score structure are unchanged.

```powershell
python -B G:\vibe\RBiOS\tools\verify_score_display.py `
  --game-root E:\MBR-2019121100\IDA `
  --dependencies E:\MBR-2019121100\IDA\RBiOS\.tools `
  --executable E:\MBR-2019121100\IDA\RBiOS\.build\active\rbscoredisplay.exe `
  --output E:\MBR-2019121100\IDA\RBiOS\.build\score-display-parity-20261110.json
```

The verifier refuses to overwrite an existing report. Its output is metadata only. This recovery establishes the integer display counter and normal-loop placement; original player rate smoothing, special mode counters, whole-game timing traces and device pixels require separate evidence.
