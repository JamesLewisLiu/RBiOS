# Original main-thread registration

`AvsThreadContext` recovers the main-thread registration decisions at
`1001AA40` and supplies a real native TLS owner to `AvsMutexes`. It registers
the calling thread explicitly; creating the C++ owner does not register a
thread or complete the preceding thread-pool bootstrap.

## Evidence

Source DLL SHA-256:
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.
Fresh Official IDA MCP evidence is retained in
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/`:

- `official-main-thread-pseudocode.json`
- `official-startup-dependencies-pseudocode.json`
- `official-startup-final-leaves-pseudocode.json`

| Original address | Recovered role |
| --- | --- |
| `100411B0` | Initialize the 40-byte task control and completion synchronization |
| `100417C0` | Initialize the 72-byte thread identity and two synchronization channels |
| `100414C0` | Link identity and control, set identity state 3, signal readiness |
| `10039690` | Bind the current native thread; Win32 retains a duplicated thread handle |
| `10039650` | Bind current thread identity in TLS |
| `100148F0` | Allocate a type-2 descriptor for task control |
| `10041160` | Bind its descriptor; zero main-thread task action/result/state fields |
| `100397F0` | Map original priority to a Windows relative priority |
| `10041230`, `100410D0` | Mark task completion and wait for completion before disposal |

Native control and identity records retain real pointer links and use native
mutex/condition-variable channels. A `thread_local` pointer records the active
owner; its identity address is registered with `AvsMutexes` only after descriptor
and priority setup succeed. Thread identities are isolated across native threads.
The adapter does not expose x86 record sizes or pointers as ARM64 objects.

## Priority behavior

The original input accepts 0 through 4. Its mapping is respectively Windows
relative priority `+2`, `+1`, `0`, `-1`, `-2`. The zero-initialized identity
priority means input 0 skips the setter in the original start routine; that
branch is preserved. Other values update the stored priority before calling
the platform adapter.

`AvsNativeThreadPriority` actually calls `SetThreadPriority` on Windows.
On Darwin/POSIX, original priority 2 maps to a real `pthread_setschedparam`
normal-scheduling request. Other Windows-relative priorities have no exact
`SCHED_OTHER` equivalent and report unsupported instead of pretending success.
Native OS failures are reported. The original Windows wrapper discards the
`SetThreadPriority` result, so this explicit native failure boundary is stricter
than that original wrapper. Tests use an injected recorder for all five original
branches and separately exercise the real normal-priority adapter.

## Completion and ownership

`complete(result)` records actual task completion state 2 and signals the native
completion condition. It must be called by the registered owner when its work
has actually completed. `stop()` requires that state and releases its descriptor
and TLS registration. An unfinished task is explicitly rejected; the module
does not label it complete or implement a synthetic successful task destructor.

The original disposal helper `100410D0` blocks until state 2. Worker wait/join
and cancellation are not implemented here. The bounded main-thread adapter
rejects unfinished disposal rather than entering a wait that its calling owner
cannot complete. Native C++ owner destruction has a separate cleanup path that
neutralizes dangling callbacks and clears TLS without emitting original task
completion. This path exists for host exception/lifetime safety and is not proof
of original game-thread shutdown.

The owner must be stopped or destroyed on its registered native thread and must
outlive descriptor callbacks that refer to it. Outstanding mutex ownership must
be released before detaching its registered identity.

## Required bootstrap still pending

`1001ABC0` first validates nonzero thread count, rounds minimum/maximum stacks
to 4096-byte boundaries, initializes a finite 40-byte task pool, clamps initial
worker count, intersects CPU affinity with discovered processor count, and
builds the native worker pool through `10041E40`. The configuration includes
stack limits, affinity, initial worker count, expiry policy, and worker stack
size. Its warning path for expiry count 1 is not a standalone success path.

The worker pool, task queue, CPU discovery/affinity adapter, worker stack
allocation, cancellation, and shutdown must be recovered before the complete
`10029410` thread bootstrap can pass. This module supplies the real current-thread
owner needed after that stage; it does not bypass it.

## Validation

Native tests pass TLS isolation, all five priority decisions, actual normal
priority application, type-2 descriptor ownership, mutex operations through the
registered identity, descriptor exhaustion, explicit priority failure, completion
requirements, and native cleanup after a failed start.

`tools/verify_avs_thread_context.py` executes original main-thread initialization,
linking, TLS-binding, descriptor allocation, and priority-selection instructions
for all five inputs. It verifies original control/identity links and states and
compares descriptor token, priority field, setter count/value, and TLS presence
with the native implementation. Only OS operations and diagnostics are hooked;
no original process, worker thread, or network connection starts.

Report:
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/avs-thread-context-report.json`.
