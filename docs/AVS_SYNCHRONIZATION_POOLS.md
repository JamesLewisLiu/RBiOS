# Original synchronization pool bootstrap

`AvsSynchronizationPools` allocates the four resource arrays following the mutex pool in original `10029410`. These are real bounded arenas with real type-1 descriptors; their existence does not claim that condition, event or semaphore operations have been recovered.

| Original function | Full configuration offset | Element bytes | Pool |
| --- | --- | --- | --- |
| `1000EC00` | 108 | 20 | Condition |
| `10018B60` | 110 | 24 | Event |
| `10019470` | 112 | 4 | Semaphore |
| `1001BF70` | 114 | 20 | Condition state |

Each count is an unsigned little-endian 16-bit field. The original functions receive the thread section beginning at full offset 72, so their local offsets are 36, 38, 40 and 42. Each calls the unchanged `10039D20` allocator wrapper in the listed order. Zero counts and allocation failure reach the original fatal handler. Native construction throws `InitializationError` identifying the failed stage, then releases any earlier native-owned pools. It never reports readiness for a failed allocation.

`state()` exposes each pool's descriptor, count, element size and original arena offset/advance. `originalArenaBytes()` is the sum of the original wrapper advances. Those figures describe the original packed arena; they do not claim to measure total C++ object, synchronization or allocator overhead. Native pool objects own their arenas independently and keep native-width metadata. Their actual allocation/free behavior is covered by the resource-pool recovery.

## Deferred mutex binding

Original thread initialization creates the task pool and initial native workers before constructing the mutex pool. `AvsMutexes(descriptors)` therefore creates a native thread-identity registry with no mutex pool. Early workers can register/unregister their identities, while `create()` returns zero, `available()` reports zero and `resourcePoolBound()` remains false.

After constructing the real 16-byte resource pool, the owner calls `bindResourcePool(pool)`. Binding validates the count, element size and descriptor service, constructs native mutex metadata, and only then publishes the bound state under the owner lock. Failure leaves the registry unbound and retains its thread registrations. Rebinding is rejected. The supplied pool must outlive the mutex owner; destroy all holders/waiters and worker registrations before owner teardown. Existing immediate and standalone constructors retain their behavior.

Original `10022B80` also stores the 16-bit mutex-delay setting at full configuration offset 118. Pass this value to `bindResourcePool(pool, delayMilliseconds)` (or an immediate constructor). After every successful lock, including recursive acquisition, `10022940` calls `10039630(delay, 0)` while ownership remains held. Nonzero values map to `std::this_thread::sleep_for`; zero skips both sleep and yield. Invalid handles and unregistered threads never sleep. An injectable `Sleep` function makes this ordering testable without real delays; an exception from that native injected function rolls back only the just-acquired recursion before propagating.

## Validation and evidence

Official IDA MCP evidence is retained in `official-thread-pools-pseudocode.json`, `official-startup-services-pseudocode.json` and the resource-pool evidence files. Input SHA-256 is `d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91` for `libavs-win32.dll`.

`AvsSynchronizationPoolsTests` exercises descriptor order, finite allocation/reuse, arena accounting, zero-count failure at every stage, descriptor exhaustion and partial-construction unwind. It also registers a real native thread before mutex binding and checks failed-binding recovery, rebind rejection, mutex use and teardown order. These tests need no proprietary files.

`tools/verify_avs_synchronization_pools.py` executes all four original boot helpers plus `10039D20`, `1000F900`, `1000F6A0` and original descriptor allocation instructions. **67 configurations / 268 boot stages** match native descriptor identities, capacities, ordering and arena advances. Only native Windows critical-section initialization, descriptor locking, memset and logging are supplied by hooks. This is an offline instruction comparison, not execution of the game or a server connection. Native failure unwind and thread behavior are tested separately.
