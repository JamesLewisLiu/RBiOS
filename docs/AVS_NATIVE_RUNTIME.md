# Native AVS thread startup owner

`AvsNativeRuntime` connects the recovered descriptor and thread initialization prefix to the default Apple entry. All construction, main-thread TLS binding and teardown execute on one dedicated native thread. The GCD queue only performs file/XML work and waits for the startup report; its workers do not own AVS TLS.

## Source order and real resources

The order comes from `libavs-win32.dll` `10013BC0`, `10029410`, `1001ABC0` and their called helpers:

1. Allocate the descriptor service using full configuration offset 68.
2. Derive native processor/stack/count policy, prepare native TLS and synchronization.
3. Clear the completion registry and allocate the finite task record pool.
4. Create the configured initial native workers, with actual start handshakes.
5. Allocate and bind the mutex resource pool, including the original lock delay.
6. Allocate condition, event, semaphore and condition-state pools in source order.
7. Allocate alarm records and enter the private alarm worker under reserved token `0x40003`.
8. Align the original arena extent, create the descriptor-service mutex and bind the original main-thread context.
9. Stop at the unrecovered filesystem bootstrap `10022CD0`.

The reserved alarm uses no ordinary task descriptor or pool checkout. The main task and worker callbacks share one TLS model. `AvsTasks` borrows the preallocated task arena and completion registry, avoiding the extra resource descriptor that creating a second independent service would introduce. Without other allocations, the six type-1 resource descriptors occupy indices 4 through 9, the service mutex takes index 10, and the main task takes index 11.

The thread-prefix report is published only after these resources actually exist and the alarm task has entered. Any native exception or failed allocation reports its current stage without readiness. It does not report complete AVS startup. Condition/event/semaphore pool allocation is implemented; all exported object operations are not yet recovered.

## Platform and ownership boundaries

Original x86 arena accounting is kept separately from native C++ memory. Task controls, callbacks and synchronization use native-width pointers and native object lifetimes. The original descriptor, task, worker, resource and alarm extents must fit the supplied AVS heap size. Extra intermediate extent checks reject unsafe configurations before native construction; the source checks the aggregate after thread initialization.

The Windows 32-bit process affinity mask is read on Windows. Apple supplies its online processor count, represented as a low-bit mask capped at 32. Native TLS storage replaces `TlsAlloc` and `TlsSetValue`; a statically initialized C++ TLS slot does not pretend to allocate a Windows TLS index. Native recursive synchronization and the monotonic millisecond clock replace Windows critical-section spinning and multimedia timer-resolution configuration. The source spin policy remains recorded, but is not an assertion that a pthread mutex spins exactly that many times. Unsupported affinity or relative worker priority remains an explicit worker status, as in the component adapter. Main-thread priority failure remains fatal to this prefix.

Stopping joins the private alarm and pooled workers and releases descriptor-service resources. The main-thread context detaches on the same native thread. Cancellation is checked at each initialization boundary; waiting for readiness does not hold the lifecycle mutex. Self-join is rejected before acquiring that mutex. On teardown, current readiness and descriptor handles are cleared while the historical trace remains. This is cancellation and partial-initialization cleanup at an unrecovered boundary, not a recovered full-game shutdown. In particular, the adapter does not call `main.complete(0)` to invent a successful game exit. Retry first joins the old owner. Apple releases resource-directory access only after stopping that owner.

## Validation

`AvsNativeRuntimeTests` verifies real startup order and descriptor indices, owner-thread affinity, readiness, concurrent-caller stop, each stage's injected failure, zero capacities, descriptor exhaustion before service/main startup, invalid priority, heap overflow and repeated startup. Original callback, mutex delay and passive-pool behavior have separate instruction oracles. The aggregate boot oracle passes 24 cases, comparing original ordering and arena/descriptor values with explicit native OS boundaries; it does not execute filesystem boot or the game.

Official IDA MCP evidence is retained under the E-drive analysis workspace: `official-runtime-platform-pseudocode.json`, `official-runtime-registry-pseudocode.json`, `official-runtime-lifecycle-pseudocode.json`, `official-runtime-self-pseudocode.json`, and the earlier thread-pool/alarm files. Source DLL SHA-256 remains `d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.

Remaining barriers include full logging/filtering and property services, filesystem boot/backends, heap/network stages, EA3, original TEST and game scene ownership. No original resources/configuration were changed, and no server traffic was performed. Host or simulator tests cannot establish M1/iPadOS 14.6 gameplay acceptance.
