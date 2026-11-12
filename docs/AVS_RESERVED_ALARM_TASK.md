# Reserved AVS alarm task

`AvsReservedAlarmTask` connects the recovered alarm dispatcher to a real native
worker and the original reserved descriptor `0x40003`. It does not allocate a
regular task descriptor or consume a shared task/worker pool slot.

The source path is `10040700` → `10041580` (native worker start), `10041160`
(task fields bound to reserved descriptor), and `100414C0` (assignment). Raw
argument pushes at `100407AB` through `100407B1` establish stack 4096, priority
0, affinity -1 and expiry 1. The source worker owner pointer is null, so this
path has no shared-pool initialization, checkout, idle-list insertion or
retired-list callbacks. The private-owner mode of `AvsNativeWorkerPool` shares
only native thread, synchronization and failure-containment implementation.

The native worker binds its actual stable worker-record address to the shared
`AvsThreadLocal` service, registers that identity with `AvsMutexes`, and exposes
descriptor `0x40003` for the duration of the alarm action. Alarm callbacks can
therefore use the same identity/TLS path as the main thread and regular tasks.
The worker unregisters and unbinds on exit. The alarm action does not invent a
second TLS identity or report task completion before `AvsAlarms::run()` returns.
Original `10040870` returns zero after its stop loop exits; callback exceptions
propagate through the task future and remain failures.

On POSIX, the requested 4096-byte stack is rounded up to the platform minimum
and page size. Priority 0 corresponds to Windows relative priority +2; POSIX
normal scheduling has no exact equivalent. `priorityApplied` reports whether
the native adapter applied the mapping, without turning unsupported scheduling
into a false successful priority operation. Affinity -1 requests no pinning.

## Lifecycle and ownership

Dependencies must outlive the reserved task owner. `start()` creates the actual
private worker, binds the reserved descriptor, assigns the recovered loop, and
waits for actual entry or failure. `requestStop()` wakes the alarm dispatcher.
`wait()` observes real completion and propagates exceptions. `stop()` requests
shutdown, observes completion, waits for worker exit, joins the native thread,
and neutralizes its reserved descriptor callback/payload. It never releases the
reserved token into the regular descriptor free list. Original descriptor
cleanup invokes a callback that waits for actual completion; it does not
silently request an alarm stop.

Fresh source shutdown `10040680` confirms this ordering: set stop and post the
alarm semaphore under its mutex, wait for task completion via `100410D0`, invoke
the post-completion helper `10041560`, destroy the private worker (`100415E0`), then destroy the task
control (`10041190`) and alarm synchronization objects. The native wrapper
follows that order; its dependencies own the alarm synchronization objects and
must be destroyed only after this wrapper is stopped. Native repeated stop is
idempotent, while the original shutdown routine assumes one service shutdown.

Cancellation before start is explicit and does not manufacture a completed
task. Restart is unsupported because the source alarm boot and its stop flag
are a single service lifetime. Waiting or joining from an alarm callback is
rejected. A callback may call `requestStop()`, but another thread must join and
destroy the owner. Destroying the owner from its own active callback terminates
instead of allowing the executing worker to reference freed memory.

The owner reports task entry, actual completion, joined state, failure, worker
identity, requested settings and native priority application. A completed loop
is not evidence that filesystem boot, TEST services, or the original game
module has completed startup.

## Validation

`AvsReservedAlarmTaskTests` runs real native workers. It verifies callback TLS
and mutex ownership, reserved-token retention, actual completion, self-wait
and restart rejection, cancellation before startup, an occupied reserved
descriptor, and callback failure while holding a mutex. Failure cleanup releases
the current worker's locks and propagates the failure; it does not fabricate a
successful original task result. The tests also check teardown leaves no stale
owner payload in the reserved descriptor and preserves unrelated ownership.

The alarm engine's separate 254-case original-instruction oracle covers queue
and callback behavior. These native lifecycle tests do not claim to emulate
Windows thread scheduling or compare the complete boot routine instruction for
instruction.
