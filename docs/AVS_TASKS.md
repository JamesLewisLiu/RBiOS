# Original AVS task admission and recycling

`AvsTasks` restores export `091` (`1001A7D0`) using `AvsResourcePool`,
`AvsDescriptors` and the actual native worker pool. Task records have finite
capacity and own a native future rather than truncating native pointers into the
original 40-byte x86 record. Descriptor tokens retain their original 32-bit layout.

## Source order and errors

The source DLL SHA-256 is
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.
Fresh Official IDA evidence is retained in `original-boot-review`:
`official-task-admission-pseudocode.json`,
`official-task-admission-leaves-pseudocode.json`,
`official-task-and-alarm-pseudocode.json` and
`official-worker-retirement-pseudocode.json`.

| Address | Native correspondence |
| --- | --- |
| `1001A7D0` | Validate procedure, nonzero page-aligned stack and priority 0..4; apply the original unsigned stack-clamp expression; allocate task control; check out worker; allocate type-2 descriptor; bind and dispatch |
| `10039CE0`, `10039CB0` | Allocate/release finite original resource-pool slots; placement-construct/destroy ARM64-safe control payloads |
| `100411B0`, `10041160` | Initialize native result synchronization and bind the procedure, argument and descriptor |
| `10041B00` | Select idle head for small stacks, preserve expiry on reuse, reclaim retired records, create from free head, or restart idle head for a larger stack |
| `100414C0`, `10041130` | Execute the actual procedure on its assigned worker and publish the result |
| `1001AB10`, `100410D0`, `10041190` | Descriptor destruction waits for real task completion before destroying task control and returning its finite slot |

Invalid arguments return `0x80010016`; allocation/admission failures return
`0x8001000C`. `lastFailure()` exposes the failed native dependency without changing
the source error code. Descriptor success uses the source signed-positive test;
the recovered token allocator occupies bits 0..30, including its generation.

The ordering has an observable failure quirk: if descriptor allocation fails,
the task record is released but the already checked-out worker is not returned to
the idle list. A subsequent admission can therefore fail despite a free task
record. This behavior is preserved and instruction-tested. Worker creation
failure likewise does not restore the removed free record or the affinity usage
increment. No retry queue or alternative selection rule is introduced.

Idle reuse does not reset the worker's remaining uses. Newly created workers
with a stack above the default receive one use. A larger-stack request restarts
an idle worker only after the retired and free lists are exhausted. Retirement
releases its affinity usage; reclamation reverses the retired list onto the free
head exactly as in the source. The pool mutex is released before native creation
or recreation, and returned-idle publication precedes native `waitIdle` success.

## Native lifetime boundaries

`wait` copies a shared result before releasing the task-service lock, so another
owner can destroy the completed record without invalidating an existing waiter.
`destroy` invokes the real type-2 descriptor callback; it waits without holding
the service lock. Procedure exceptions propagate through wait and destroy, and
the finite record is still returned. Worker exception containment is documented
in `AVS_NATIVE_WORKERS.md`.

Self-wait and self-destroy of an unfinished task are explicit native errors.
Waiting for an older completed task on the same recycled worker is permitted.
Native owner destruction drains existing tasks; it does not create successful
results for failed game procedures. The owner must outlive calls to its API.
Bulk descriptor cleanup retains the original lookup-visible token/payload while
this service is alive. A bounded per-descriptor-index binding table lets native
owner teardown neutralize any retained callback and payload after records have
already been released, without modifying descriptors reused by another owner.

Tasks now dispatch the recovered four-handler completion registry before their
result is published. Registry mutations and callback ordering are described in
`AVS_THREAD_LOCAL_AND_COMPLETION.md`. The current task descriptor and real native
owner identity remain visible through the shared TLS provider during the action
and all completion callbacks. Callback failures propagate through the task result.

The borrowed-pool constructor lets the enclosing boot owner construct its type-1
task pool before workers, matching `1001ABC0`. Use `recordSize()` and
`recordAlignment()` for native payload storage, and optionally pass an external
completion registry. Both borrowed owners must outlive this task service. The
constructor checks same descriptor ownership, configured capacity, empty slots,
payload size and alignment. Native task destruction returns slots without
destroying a borrowed pool or registry. The convenience constructor still owns
its own pool for standalone tests. Other `10029410` service constructors remain
required; these components do not bypass them.

## Validation

Native regressions cover finite task records, real procedure results, argument
validation, stack clamping, descriptor exhaustion, the stranded-worker quirk,
exceptions, recycling through generation wrap and waiting on a completed task
from its reused worker. The existing worker tests cover native locks, callback
ordering, shutdown, self-wait and held-mutex failure containment.

`tools/verify_avs_task_admission.py` executes original x86 worker instructions for
48 configurations and 4,800 admission/recycling commands. It compares exact
selection, state, remaining uses, stack, priority and affinity against real native
worker threads. OS creation/synchronization are boundary hooks; original list and
affinity operations execute unchanged.

The same oracle runs export `091` twice with exhausted descriptors. It compares
the original failure code, returned task slot and retained worker state against
the native finite resource/descriptor implementation. Only resource allocation,
descriptor exhaustion and logging are additionally hooked for those two calls.

Report:
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/avs-task-admission-final-report.json`.
