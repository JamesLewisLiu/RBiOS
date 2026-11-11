# Native execution of recovered AVS worker behavior

`AvsNativeWorkerPool` creates real native threads with recovered initial slot
order, explicit task assignment, completion callback order, expiry, and shutdown.
It is not the complete original task scheduler. The caller must select an idle
original worker slot before calling `assign`; the module does not invent a queue,
load-balancing rule, automatic gameplay task, or successful boot-stage callback.

## Source correspondence

The source DLL SHA-256 is
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.
Evidence is retained in the local `original-boot-review` directory in
`official-thread-bootstrap-pseudocode.json`, `official-worker-bodies-pseudocode.json`,
`official-worker-loop-pseudocode.json`, and `official-worker-routing-pseudocode.json`.

| Address | Restored behavior |
| --- | --- |
| `10041E40` | Initialize all worker records before starting initial workers; start from the highest free index downward; limit initial count to capacity; unwind in reverse record order |
| `100417C0` | Native worker synchronization exists before execution starts |
| `10041580` / `1004158C` | Record stack, priority, affinity, expiry, and initial state 1 before creating the thread |
| `100396E0` | Create native thread, apply platform policy before releasing its entry gate, report native creation failure |
| `10041660` | Bind worker identity, wait for a task, execute it, update worker state, and exit when stopped |
| `100416A0` | States 1/2 wait, state 3 executes the assigned task, states 0/4 exit |
| `10041130`, `10041230`, `100418C0` | Execute action, invoke up to four completion callbacks in order, then publish task result |
| `10041750` | Zero expiry means no usage limit; otherwise decrement after task completion and retire at zero |
| `10041624`, `10041564`, `100415E0` | Signal stop, join a started worker, and release its native handle |

Initial worker construction arguments are verified from raw stack arguments,
including values omitted by the decompiler's prototype: worker record, stack
size, original priority, selected CPU index, and expiry count. The task action
and callback closures are adapter representations of the original function and
argument pairs, not replacements for game logic. No action runs merely because
the pool was constructed.

## Native platform behavior

Windows uses `_beginthreadex` with the requested stack size. POSIX uses
`pthread_create` and `pthread_attr_setstacksize`; rejected native stack sizes
remain failures. A native condition-variable gate prevents task execution before
policy setup and identity registration complete. Construction waits for each
worker to acknowledge registration, so a failed identity binding cannot be
reported as an initialized worker.

Windows applies the original relative priority mapping and disables priority
boost. POSIX supports original priority 2 through normal scheduling; other
relative priorities explicitly fail because they have no exact mapping here.
Windows and Linux attempt the selected CPU pin. Darwin does not expose an
equivalent hard CPU pin through this adapter, so `affinityApplied` remains false.
The original `100396E0` ignores affinity-setting failure; native creation likewise
continues but makes the outcome inspectable. It does not claim that unsupported
pinning was applied.

The original Windows priority wrapper also ignores an OS setter failure. This
native implementation explicitly reports priority failure and unwinds workers,
which is a deliberate platform error boundary rather than a fabricated success.

## Identity and task lifetime

Workers bind a real thread-local identity and explicitly register it with
`AvsMutexes`. `currentWorkerIdentity()` returns that identity only on the worker.
It is currently separate from `AvsThreadContext::currentIdentity()`; the original
exported lock and thread-query adapters still need a unified TLS provider before
arbitrary migrated game calls can use either owner interchangeably. A game call
must not silently treat the other provider's zero identity as registered.

Task futures publish the action result after completion callbacks. An exception
from the action or a callback is propagated through the future and retires the
worker instead of reporting successful completion. `waitIdle` waits for complete
retirement, including identity unregistration, and propagates asynchronous worker
failures. `status.failed` preserves asynchronous failures. Before completing a
future successfully, the native worker checks that its task released every AVS
mutex it owns. If a task throws or returns with outstanding ownership, it fails
the future and releases only that native thread's locks on the same thread,
then unregisters and retires. Locks held by other threads are untouched. This
is native exception containment; it is not an original successful task cleanup
or a substitute for the task's missing game behavior.

Stop prevents further assignments, signals workers, waits for already executing
actions, joins threads, and checks native join/handle-close results. A queued
action stopped before dispatch receives an explicit cancellation exception.
Failed joins do not clear `started` or free live worker state. Calling stop from
one of the pool's own workers is rejected before shutdown state changes. Explicit
waiting for that worker's own idle state is rejected as well. The explicit
failure-containment tests cover both throwing and returning while holding a
recursive mutex, preserve failure in the future and worker status, and verify
that another thread's mutex remains owned. If containment itself fails, shutdown
propagates the cleanup error after joining instead of reporting success.

Explicit
stop allows errors to be reported; a destructor cannot safely recover from a
native join failure and must not destroy a still-running worker.

## Validation and remaining integration

Native tests pass real thread creation, absence of unsolicited work, initial slot
order, thread identity, mutex operations, four-callback order, action results,
unlimited and finite expiry, action exceptions, invalid priority, explicit
self-join rejection, stopping while a task is executing, and complete joins.

The original instruction oracle passed **17 pool creations** and **74 task-loop
transitions**. It executes original worker/control initialization, descriptor-free
task binding, callbacks, result publication, and expiry branches. OS operations
are captured; the comparison executable uses real native threads. The oracle
does not execute a live original process or network operation.

Reports:
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/avs-native-workers-final-report.json`
and the bootstrap policy report described in `AVS_THREAD_BOOTSTRAP.md`.

Original task admission/selection, idle and retired list ownership
(`10041A90`/`10041AC0`), affinity decrement on retirement, expired-slot recycling,
future worker creation, task-record allocation, and full thread-context export
integration remain pending. In particular, `100419A0` enumerates task state; it
is not a task-submission function. These missing services must be recovered from
their callers before the complete `1001ABC0`/`10029410` stage can be declared
ready. The aggregate also initializes mutex, condition, event, semaphore, and
alarm services; this worker module does not bypass them.
