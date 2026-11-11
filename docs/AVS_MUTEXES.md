# AVS mutex resource ownership

`AvsMutexes` supplies a finite native mutex resource pool and connects actual
resources to the recovered `AvsDescriptors` lifecycle. It does not advance the
thread bootstrap or substitute a successful return for an unimplemented stage.

## Evidence and mapping

Fresh Official IDA MCP evidence is retained under
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/` in
`official-startup-dependencies-pseudocode.json`,
`official-startup-services-pseudocode.json`, and the earlier
`official-services-leaves-pseudocode.json`. The source DLL SHA-256 is
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.

| Original function | Behavior | Native counterpart |
| --- | --- | --- |
| `10022B80` | Initialize mutex pool from configuration `uint16` at `+0x22`; reject zero count; retain optional wait delay at `+0x2E` | Constructor accepts explicit finite count; configuration import and delay remain caller work |
| `10039CE0`, `10039CB0` | Serialize resource allocation/free with a critical section | Pool mutex and ordered free-slot stack |
| `100399F0` | Create an initially nonsignaled auto-reset event; initialize owner, lock state and recursion count | Allocate real `std::recursive_mutex`; allocation errors return failure |
| `07B / 10022AB0` | Allocate resource, initialize it, create type-3 descriptor; unwind resource if descriptor creation fails | `create` |
| `10022B30`, `100399D0` | Destroy native event and return its resource to the pool | Descriptor disposal callback |
| `013 / 100150E0` | Create the descriptor-service mutex, save its token, fatal error on failure | `startDescriptorService`; throws on failure |
| `023 / 10015090` | Invoke service-mutex destructor directly and clear global token | `stopDescriptorService` |
| `10039930`, `100398F0` | Recursive ownership; contention waits until final release signals a waiter | Real native recursive lock/unlock |

Original pool slots are 16-byte resources, with allocator headers and alignment
outside them. Native slots retain index order and free-stack reuse but contain
native-width resource references, not copied Win32 event handles or x86 layout.
Descriptor allocation and resource exhaustion both return zero. Resource
allocation is unwound if no descriptor is available. Native owner destruction
neutralizes outstanding payload callbacks before their backing records disappear.

The original stop operation does **not** free its descriptor: the descriptor's
payload can still be looked up after its resource has been disposed. That
observable behavior is preserved. Stop is a terminal lifecycle operation; using
the old descriptor after stop or restarting service ownership needs the original
owner's full shutdown sequence. Native owner destruction additionally clears
remaining callbacks so destruction of the C++ object cannot leave dangling native
function payloads in the descriptor pool.

## Thread and synchronization boundary

Original `10022940` obtains a registered thread object through Windows TLS before
entering `10039930`. The native adapter requires explicit
`registerCurrentThread(originalThreadIdentity)` and verifies that `lock` receives
that identity on the same native thread. No implicit main-thread registration or
synthetic thread token is installed. Tests use explicitly synthetic identities;
the actual thread owner must supply its recovered identity during integration.

The native primitive preserves blocking and recursion, but does not reproduce
the exact Win32 event scheduler or undocumented waiter fairness. The optional
configured delay after locking is not applied by this module. Unlock by a
different native thread, missing registration, or destruction with holders or
waiters reports an explicit error instead of invoking native undefined behavior.
Resource destruction must be externally sequenced after all users are quiescent.

`releaseCurrentThreadLocksAfterFailure` is a separate native exception-containment
operation used by recovered worker execution. It releases only mutex recursion
owned by the calling native thread, on that thread, so a failed task can retire
without stranding locks. It does not release another thread's locks or change a
failed task into a successful one. The original mutex export API does not imply
this operation; it is explicitly a host failure boundary.

`AvsDescriptors` currently retains its own native recursive synchronization
adapter. The exported descriptor-service mutex resource exists with real
ownership, but the two locks are not yet connected into the complete original
runtime. Creating that resource alone does not complete AVS startup.

## Remaining startup dependencies

`10029410` is an arena/bootstrap aggregator. It initializes platform state and
then calls `1001ABC0`, `10022B80`, `1000EC00`, `10018B60`, `10019470`,
`1001BF70`, and `10040700` in sequence. This module supplies only the mutex
resource foundation and does not bypass any other allocator.

`1001AA40` registers the current main thread, initializes two synchronization
objects, creates a type-2 descriptor, binds TLS, and applies the configured
priority. It is not a generic thread-spawn function. Its helpers, platform TLS
ownership, native priority mapping, thread-object lifecycle, and the remaining
bootstrap subsystems still need recovery before this stage can succeed.

## Validation

Native tests pass finite-resource exhaustion, descriptor-exhaustion unwind,
service startup/stop, explicit identity checks, nested recursion, actual
cross-thread contention, rejection of destruction while locked, and teardown.

`tools/verify_avs_mutexes.py` compares **2,004 commands** with original x86
instructions for `10022AB0`, `10022B30`, descriptor initialization, allocation,
and release. It uses four resource/descriptor capacity combinations and
deterministic allocation/destruction sequences. The oracle hooks native-event
allocation and finite-slot allocation, verifies resource initialization and
destruction ordering, and executes the original ownership/unwind branches.
It does not execute a Windows process, start original threads, or open network
connections. Native event-allocation failure is not forced by this oracle.

Report:
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/avs-mutexes-report.json`.
