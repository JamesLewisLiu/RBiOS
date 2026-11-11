# Original TEST wait-state recovery

The native core now recovers the decision made by `reflecbeat.dll` at
`0x100D5710` and its status classifier at `0x100A1610`. This is the initial
wait state installed by the TEST main initializer `0x100D5AF0`. It is a
bounded recovery of original control flow, not a replacement TEST screen
or a claim that EA3 has been initialized on iPad.

## Evidence

The source binary has SHA-256
`ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8`.
Fresh Official IDA MCP pseudocode is retained locally at:

- `E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/official-boot-test-pseudocode.json`
- `E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/official-test-readiness-pseudocode.json`

| Original address | Recovered behavior | Native counterpart |
| --- | --- | --- |
| `100D5AF0` | Installs `100D5710` as the initial TEST state, selects region-specific menu records, and performs other initialization | Context only; complete initializer is not recovered by this module |
| `100D5710` | Starts an external EA3 operation on tick zero, reads its status, and conditionally queues the main-menu handler | `pollOriginalTestReadiness` |
| `100A1610` | Preserves statuses `-22..1`; all other signed 32-bit values become `-16` | `normalizeOriginalTestStatus` |
| `100D5600` | TEST main-menu handler selected after the wait | Existing `OriginalTestMenu` recovery |
| `100DD7C0` | Constructs the MAIN MENU button and associated sprite state | Context only; this is not a readiness test |

The classifier's original switch omits an explicit `-16` case and returns
`-16` through its default branch. The native interval expression has the
same result, including the signed integer extrema.

## Exact wait behavior

1. If the scene counter at byte offset `+180` is zero, invoke
   `XE592acd00005D`, whose return value is not used here.
2. Invoke `XE592acd000070` on every update and normalize its returned status.
3. Only normalized status `1` retains the current wait state.
4. Every other normalized value, including errors, writes handler
   `100D5600` at `+160`, zeros the next-state counters at `+164` and `+172`,
   and sets byte `+176` to `1`.

An error therefore queues the main menu in the original program. The native
code preserves that branch without describing it as successful service
readiness. The scene counter belongs to the original scene owner; this helper
does not increment it. Repeated calls with a zero counter repeat the begin
operation, matching the original function.

`OriginalTestReadinessSource` requires explicit implementations of both EA3
operations. There is no default ready status, stub success callback, timeout
fallback, or implicit network request. Native code returns the semantic
`queueMainMenu` decision instead of storing an x86 function address in an
ARM64 pointer. A future scene owner must apply that decision, including the
counter resets and pending-transition flag, using its native callback types.
When the decision is false, the existing pending state must remain unchanged.

## Validation

`OriginalTestReadinessTests` checks status classes, signed extrema, first-tick
call ordering, repeated zero ticks, and the error-to-menu branch.

`tools/verify_original_test_readiness.py` loads the original DLL into Unicorn
and executes the original instructions at `100D5710` and `100A1610`. Only
the two external EA3 operations are intercepted. The begin hook returns a
poison value to verify that it does not determine the next branch; the status
hook supplies each synthetic test value. No target process or external
connection is started.

The comparison passed **7,698 cases**: six signed scene-counter values,
all statuses from `-128` through `128`, the signed extrema, and 1,024
deterministic random statuses. Native normalized status, queue decision, and
EA3 call trace matched the original execution. The oracle additionally checks
the original scene writes, preservation of all other bytes in the sampled
256-byte scene object, and `ret 4` stack cleanup.

The local report is
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/original-test-readiness-report.json`.
Reproduce after building `rboriginaltestreadiness` (or supply the locally
compiled test executable) using a new report path:

```powershell
python tools/verify_original_test_readiness.py `
  --game-root E:/MBR-2019121100/IDA `
  --dependencies E:/MBR-2019121100/IDA/RBiOS/.tools `
  --executable <path-to-test-executable> `
  --report E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/new-readiness-report.json
```

## Remaining boundary

The actual implementations behind the two EA3 exports, original scene-owner
transition dispatch, TEST button rendering, hardware subscenes, and full TEST
initializer are not supplied by this module. It does not execute AVS2 boot,
open a network connection, touch NVRAM, or expose a fabricated TEST menu.
The oracle validates the recovered decision against the original instruction
stream; it does not establish complete game startup or device behavior.
