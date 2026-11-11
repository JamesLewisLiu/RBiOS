# AVS finite resource pools

`AvsResourcePool` restores the array allocator used by the original AVS resource wrappers. It owns real aligned memory, consumes one real type-1 AVS descriptor, provides finite allocations, and serializes its operations. Task controls can be placement-constructed in its payloads; allocation failure returns null without admitting work.

## Original correspondence

| Original address | Recovered behavior |
| --- | --- |
| `1000F900` | Validate layout, align the first payload, initialize the array, allocate its type-1 descriptor |
| `1000F6A0` | Initialize ascending free indices; clear active list, cursor and peak usage |
| `1000F830` | Pop free head, append active tail, update usage, clear the entire padded payload |
| `1000F6F0` | Validate a live payload, unlink active entry, push free head; return its index |
| `1000F340` | Counts, statistics, pointer/index queries, reset and shared-cursor iteration |
| `1000EF00` | Resolve a type-1 array descriptor |
| `10039D20` | Initialize wrapper synchronization and an eight-byte-aligned array; advance the caller's arena |
| `10039CE0` / `10039CB0` | Lock around allocation / release |
| `10039C80` | Lock around statistics query **2**, not reset |

Official IDA MCP evidence is retained in `official-resource-pool-pseudocode.json`, `official-resource-pool-leaves-pseudocode.json` and `official-worker-retirement-pseudocode.json` under the local analysis directory. The input is `libavs-win32.dll`, SHA-256 `d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.

## Layout and ordering

The original header is 32 bytes. Each slot has two 32-bit list indices followed by its payload. Given alignment `a`, the first payload is `align_up(arena + 40, a)`, and the slot stride is `align_up(element_size + 8, a)`. The first payload's alignment can therefore add padding before the header. The wrapper's eight-byte-aligned arena advance is `(count * ((element_size + 15) & ~7) + 51) & ~15`.

The initial free list is ascending. Active entries preserve allocation order. Released entries return to the free head, so subsequent allocation reuses them in reverse release order. Allocation zeros `stride - 8` bytes, including padding. Release does not erase data. Reset does not erase payloads or destroy native objects; it resets free/active lists, cursor and peak usage.

Query opcodes 0/1 report active/free counts. Opcode 2 returns capacity, peak active count and current active count. Opcodes 3/4 test pointer/index allocation; 6 maps a payload to its index; 7/8 map an index to a payload with optional allocation validation. Opcodes 9–12 walk the active list using one shared cursor. Reset is opcode 5. Empty first/last queries leave the cursor unchanged, matching the original. A caller must synchronize an entire multi-call traversal if it requires a stable snapshot; `activeIndices()` provides an atomic snapshot for native consumers.

## Native ownership boundary

The pool owns its arena and keeps list indices in native metadata instead of embedding 32-bit pointers in game structures. Payload addresses remain native-width and retain the original stride and alignment. A recursive native mutex replaces the Windows critical section. The descriptor table must outlive the pool. The caller must finish all concurrent operations and destroy placement-constructed objects before releasing, resetting or destroying their pool. Descriptor cleanup does not destroy C++ objects or reclaim the owner's arena.

Layout rejects zero counts/sizes, non-power-of-two alignment below eight, and invalid arena alignment. Counts at or above `0xfffffffe` clamp to `0xfffffffd`, as in the original. Native layout computation rejects overflowing slot strides instead of allowing x86 arithmetic to wrap into an unsafe allocation. Invalid/double release throws instead of entering the original fatal handler. Constructor allocation or descriptor failure throws; exhaustion of an established pool returns null. These native ownership/error adaptations do not claim to reproduce the original process-fatal logging path.

This module restores allocation infrastructure. It does not claim that allocating a condition, event, semaphore or alarm pool implements those objects' operating-system behavior. Their operations require their respective recovered services.

`AvsMutexes(descriptors, pool)` consumes real slots from a borrowed pool with at least 16-byte elements and a capacity within `uint16_t`. The pool must belong to the same descriptor service and outlive the mutex owner. Native mutex records remain outside the original byte payload to preserve native pointer widths. Every create failure after slot allocation returns the slot, and disposal returns it exactly once. Failed descriptor creation still contributes to the pool's peak usage because the original resource allocation occurred before the failure. Owner destruction retires its type-3 descriptors and returns all its slots while leaving the borrowed type-1 pool alive. A later mutex owner can reuse the pool. External consumers may hold other slots, but must not reset the pool or release the mutex owner's slots.

## Validation

`AvsResourcePoolTests` exercises finite exhaustion, FIFO/LIFO ordering, padded zeroing, non-clearing reset, statistics, cursor traversal, exact pointer checks, invalid frees, alignment, descriptor lifetime/failure and 16,000 allocations from eight native threads. It requires no proprietary files.

`tools/verify_avs_resource_pool.py` runs unchanged x86 creation, initializer, allocate, release, query and descriptor-validation instructions in Unicorn, then compares their results with the native executable's batch mode. The retained final report records **18,894 matching commands**, including 24 element-size/count/alignment combinations and randomized allocation/reset/query sequences. Only memset, logging and descriptor lock primitives are hooked; allocation decisions and list mutations execute original instructions. The oracle never starts the game, opens a server connection or modifies original resources.

Example (use an analysis-directory output path):

```text
python tools/verify_avs_resource_pool.py --dll <libavs-win32.dll> --dependencies <python-dependencies> --executable <AvsResourcePoolTests> --report <new-report.json>
```

The oracle does not execute fatal invalid frees or overflowing 32-bit layouts. Native tests cover the bounded rejection paths. Apple/iPad compilation and on-device behavior remain separate integration checks.
