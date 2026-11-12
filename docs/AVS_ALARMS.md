# Original AVS alarm service

`AvsAlarms` recovers the finite alarm records and dispatcher from the original
`libavs-win32.dll` (SHA-256
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`).
The source addresses below were inspected using Official IDA MCP, with raw x86
instructions used to resolve clock registers and signed modular comparisons.

| Source address | Recovered behavior |
| --- | --- |
| `10040BC0` | Fixed 40-byte record pool, reverse-index free-list initialization |
| `100405E0`, `10019000` | Record allocation and type-8 public descriptor creation |
| `10040490`, `10018E10`, `10018D30` | Repeating schedule, prior-active return, set-if-stopped behavior |
| `10040450` | Stop without immediate queue removal |
| `10040570` | Immediate release or deferred retirement while dispatching |
| `10040A60` | Queue cleanup, deferred reclamation and modular minimum deadline |
| `100409C0`, `10040960` | Reverse queue callback order, reschedule from actual dispatch time |
| `10040D00` | Extend a wrapping 32-bit millisecond clock |
| `10040870` | Mutex/semaphore wait, dispatch outside lock, shutdown checks |
| `10018F70` | Detached procedure record, retired after its first dispatch |

The original `set_once` export does not make a one-shot alarm. It retains an
already active schedule. A callback returning false clears its deadline, even
if that callback rescheduled itself. Destroying an alarm during dispatch marks
it retired; another callback already present in the due list still executes.
An inactive, unqueued record destroyed during dispatch remains allocated until
service teardown: the recovered queue cleanup cannot see it. This source
behavior is preserved rather than silently repaired.

At native owner teardown, all retained descriptor bindings into the alarm pool
are neutralized before the records are destroyed. This includes deferred tokens
left lookup-valid by descriptor bulk cleanup, even when their alarm records
have already been reclaimed or reused. Live source cleanup semantics remain
unchanged; owner teardown does not call a stale record disposer a second time.

Pointers and callbacks use native widths; public descriptors remain 32-bit.
`std::recursive_mutex` and `condition_variable_any` implement the native mutex
and counting-wakeup boundary, including wake tokens posted before a wait.
The constructor requires an explicit semaphore limit. Original `10039530`
supplies `nr_thread` to the maximum used by `10040700` (50 in the inspected
configuration); the alarm-record capacity is a separate value. Wake tokens
saturate at this limit, matching the ignored failure of `ReleaseSemaphore` when
full. `pendingWakeSignals` exposes the count for native boundary diagnostics.
The default clock is monotonic milliseconds, truncated to the original 32-bit
input and extended by the recovered wrap counter. Tests inject that raw clock.
Native callback exceptions stop dispatch and propagate to the worker owner;
the original C ABI has no C++ exception contract. Invalid destroy handles return
the native invalid-handle error; the source's logging-function return is not a
portable public return contract.

## Reserved worker integration boundary

`run()` is a blocking worker entry. It deliberately does not create or allocate
a regular AVS task. Original boot `10040700` binds reserved descriptor `0x40003`
through `10041160` and uses the private worker start with priority `0`, affinity
`-1`, expiry `1`, and a 4096-byte requested stack. The argument positions are
confirmed by raw pushes at `100407AB` through `100407B1` and the worker field
stores in `10041588` through `100415B8`. `AvsReservedAlarmTask` now supplies that
reserved task, private native worker, and unified AVS task/TLS context; see
`AVS_RESERVED_ALARM_TASK.md`. Regular public
task allocation must not substitute a new descriptor for the reserved task.
Call `requestStop()` and join the worker before destroying the alarm service;
do not destroy the service from its own callback. This module alone does not
complete `10029410`, AVS bootstrap, filesystem boot, or game startup.

## Validation

`AvsAlarmsTests` covers pool exhaustion, descriptor errors, repeating/stop and
set-if-stopped semantics, callback order, deferred destruction, owner teardown
after descriptor cleanup and record reuse, clock wrap and
a real native blocking worker. The worker test verifies this module's native
boundary, not original reserved task admission. A native-only regression checks
that 20 progressively earlier schedules saturate a supplied semaphore limit of
3. The instruction oracle's Win32 import stub does not validate semaphore
counts or scheduling; that boundary is tested separately.

`tools/verify_avs_alarms.py` executes nine original x86 functions in Unicorn and
compares 254 operation snapshots with the native executable. It compares every
record's deadline, period and flags; free/active queue order; minimum deadline;
used/high-water counts; and callback identities/times. Only Win32 synchronization
and time imports and the caller's callback are supplied by the fixture. No AVS
alarm function is replaced. The oracle covers 64-bit periods and repeated raw
clock wraps. It does not claim to validate public descriptor wrappers, native
thread scheduling, or the complete worker bootstrap.

```powershell
python tools/verify_avs_alarms.py --game-root E:/MBR-2019121100/IDA `
  --dependencies E:/MBR-2019121100/IDA/RBiOS/.tools `
  --executable E:/MBR-2019121100/IDA/RBiOS/.build/active/rbavsalarms.exe `
  --report E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/alarm-oracle.json
```
