# Shared AVS thread identity and completion handlers

`AvsThreadLocal` provides one native TLS binding for the main-thread context,
pooled workers and private reserved workers. It stores the actual stable owner
address, the current task descriptor and whether a task control is attached.
There is no generated fallback identity. Manual low-level mutex registration
remains available for isolated host tests; production thread owners bind TLS
and register their real identity before executing original procedures.

## TLS source correspondence

The DLL SHA-256 is
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.
Fresh Official IDA evidence is in `original-boot-review`:
`official-runtime-platform-pseudocode.json`,
`official-runtime-registry-pseudocode.json`,
`official-runtime-lifecycle-pseudocode.json` and
`official-runtime-self-pseudocode.json`.

| Address | Restored behavior |
| --- | --- |
| `10039650` | Explicit native owner binding on the actual executing thread |
| `1001AA40` | Main-thread owner is bound before descriptor allocation; the resulting descriptor is attached to its task control |
| `10041660` | Worker entry binds the real worker owner before executing assigned task control |
| `1001A9F0` / export `079` | Return task-control descriptor; return `0x80010001` if TLS or task control is absent |

`self()` preserves the distinction between a missing control and an attached
control whose descriptor is zero. `currentDescriptor()` is diagnostic metadata;
it returns zero when no task is attached. `AvsThreadContext::currentIdentity()`
and `AvsNativeWorkerPool::currentWorkerIdentity()` both read this shared provider.
Source export `044` option 1 instead returns worker affinity at offset 64; it must
not be routed to the self-descriptor query.

The original allocates a Windows TLS index in `100395F0` and fails fatally if
allocation fails. This adapter uses a trivially initialized C++ `thread_local`
binding supplied by the native runtime, without dynamically allocating a Windows
TLS index. It does not report success for an attempted Windows allocation. Bind
rejects a zero or already-bound owner; updates and unbind require the exact owner.
Main-thread and worker teardown unregister mutex ownership before clearing TLS.
The original x86 worker's control pointer is represented by safe attached-task
metadata, avoiding dangling x86 pointers after a native task record is recycled.

## Original four-slot completion registry

`AvsTaskCompletionRegistry` restores the table at `1007BB64`:

| Address | Behavior |
| --- | --- |
| `10041960` / export `044` option 5 | Scan from slot zero; replace the first empty slot or matching handler; fail with `0x8001000C` if neither exists |
| `10041930` / export `044` option 6 | Clear the first matching slot; fail with `0x8001000C` if no match exists |
| `100418C0` | Read each of the four slots immediately before invocation, passing the actual task descriptor |

The early-hole rule is significant: registering an existing handler when an
earlier slot is empty creates a duplicate. Removal clears only the first copy.
Null add/remove retain the original scan behavior; neither gains an invented
argument-validation error. Registration/removal serialize their mutations, while
dispatch uses atomic per-slot reads without holding that mutex or copying a
snapshot. Consequently a callback can add or remove a later handler and affect
the same completion dispatch. Native atomic accesses avoid a C++ data race while
retaining source observable ordering.

`AvsTasks` owns a registry by default or borrows an explicitly supplied registry.
Its completion action calls the registry before publishing the future result.
Callbacks see the same real TLS identity and task descriptor as the procedure.
An exception fails the task and follows existing worker containment rather than
publishing a successful result. A borrowed registry must outlive all admitted
tasks and their owner.

## Validation

`AvsThreadLocalTests` runs actual native workers, compares the main and worker
identity accessors, operates an AVS mutex through the shared identity, checks
thread isolation, verifies descriptor visibility inside procedure and callback,
and confirms borrowed task-pool creation before workers. It also exercises the
private reserved worker mode and rejects public checkout on that mode.

`AvsTaskCompletionRegistryTests` covers empty/full slots, nulls, duplicates behind
holes, first-match removal and callback-driven live mutation.

`tools/verify_avs_completion_registry.py` compares **4,749 commands** against the
original registration/removal/dispatch instructions. Synthetic callback machine
code invokes those same original mutation functions during dispatch. It also
compares **five original self-query cases**, including missing TLS, missing task
control and attached zero/nonzero descriptors. OS TLS retrieval and diagnostic
logging are hooks only for the self-query cases.

Report:
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/avs-completion-registry-final-report.json`.
