# Native AVS descriptor lifecycle

`AvsDescriptors` recovers the original descriptor pool initialization and
allocate, lookup, replace, release, group switching, and bulk cleanup operations. Payloads use
`uintptr_t`, callbacks use native function pointers, and list links use indices.
The 32-bit descriptor token remains compatible with the original bit fields.

## Original evidence

Binary: `libavs-win32.dll`, SHA-256
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.
Fresh Official IDA MCP results are retained under
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/` in
`official-services-leaves-pseudocode.json` and
`official-services-detail-pseudocode.json`; bulk-cleanup recovery also uses
`official-descriptor-cleanup-pseudocode.json`. Original instruction listings are
`descriptor-instructions.txt` and `descriptor-lifecycle-instructions.txt`.

| Export suffix / address | Native operation | Recovered behavior |
| --- | --- | --- |
| `103 / 10015110` | Constructor | Four reserved records, ordered free list, category heads, generation zero |
| `0C4 / 100148F0` | `allocate` | Remove free head, assign generation/type token, insert category head, bind payload and destructor |
| `064 / 100146C0` | `lookup` | Match both masked index and full stored token; optionally return payload |
| `016 / 10015490` | `replace` | Replace payload/destructor on exact token match; return success flag |
| `130 / 10014700` | `release` | Unlink category record, invalidate token, return record to free list, unlock, invoke destructor |
| `0B2 / 10015040` | `setGroup` | Select one of four allocation groups and return the previous group |
| `13E / 100143C0` | `cleanupType` | Detach the current group's type list, defer individual release, invoke destructors, prepend the complete chain to the free list |
| `06E / 10014D40` | `cleanupGroup` | Detach and concatenate a group's category lists in descending type order, invoke destructors, and reclaim their chain |
| `013 / 100150E0` | Pending original resource adapter | Create descriptor-service mutex via export `07B`; record returned handle; report failure if nonpositive |
| `07B / 10022AB0` | Pending original resource adapter | Allocate/init native mutex resource and wrap it in a type-3 descriptor; unwind on failure |

The original record is 24 bytes. At offsets `+0/+4` it stores a 32-bit payload
and callback, `+8/+12` are list links, `+16` is the token, `+20` is cleanup
group, and `+21` is a deferred-cleanup flag. The native class does not copy
this packed x86 record into an ARM64 object.

The token contains the low 17-bit record index, six type bits at bit 17, and
the low eight generation bits at bit 23. The global 32-bit generation advances
on successful allocation, skips zero only when the entire counter wraps, and
then truncates to eight token bits. Tokens can therefore alias after 256
allocations. This original behavior is preserved, not silently strengthened.

The three first reserved records have type 12 and tokens `00180000`,
`00180001`, and `00180002`; the fourth has type 2 and token `00040003`.
Their owning services must install actual payloads and destructors with
`replace`. The free list begins at index 4. An exhausted pool returns token
zero without advancing generation. Missing destructor arguments on allocate
or replace select the original zero-return no-op destructor (`100514E0`).

## Synchronization and failure boundaries

The class owns a real `std::recursive_mutex`. Release returns the record to the
free list and unlocks before invoking its destructor, permitting destructor
reentry and immediate reuse of the released slot. Lookup also takes the native
lock; the original lookup relied on caller synchronization. Callback exceptions
propagate after the descriptor has been released. They do not restore the old
handle or fabricate success.

Bulk cleanup differs from individual release: it detaches and marks all affected
entries before unlocking, invokes destructors in list order, then re-locks and
returns the complete chain to the free list. Type cleanup processes newest
allocations first within the selected type. Group cleanup concatenates types
63 through 1 and preserves each type's existing list order. Individual release
of a deferred entry returns zero, avoiding duplicate destruction during callback
reentry. Allocation inside a callback cannot reuse the detached entries until
the bulk operation finishes.

The original bulk operations do **not** clear stored tokens. Lookup and payload
replacement can continue to match a detached token until its slot is reused;
release remains suppressed by the deferred flag. These details are preserved.
If a native callback throws, bulk cleanup leaves the remaining detached records
deferred and propagates the exception; it does not claim that their owners were
cleaned up. Original C callbacks do not supply C++ exception semantics.

This is a native synchronization adapter, not an implementation of the Windows
mutex resource pool behind `07B` or a successful substitute return for `013`.
The original boot dispatcher must explicitly connect its resource ownership
before declaring that stage complete.

Supported capacities are 1 through `0x1FFFC`, keeping all four reserved entries
inside the token index range. Type is masked to six bits as in the DLL;
masked type zero is rejected because the original category calculation indexes
before the table. Releasing an unbound reserved entry or an index-only free-list
token raises an explicit error rather than invoking an uninitialized pointer
or corrupting lists. Native unused payload storage is zero-initialized; the
original initialization leaves those bytes as supplied by the backing arena.
Valid allocated-handle behavior is the comparison domain.

Dynamic category allocation (`0A3`) and freeing (`0A4`), diagnostic enumeration,
and shutdown of the original mutex resource (`023 / 10015090`) remain outside
this module. Export `023` invokes the descriptor-service mutex destructor and
clears its global handle; it is not a sweep of every descriptor. The service
owner must explicitly select cleanup groups and bind the synchronization resource
before claiming the complete original shutdown sequence.

## Validation

`AvsDescriptorsTests` passes allocation, exhaustion, stale-token rejection,
replacement, wide native payload, callback reentry, eight-bit generation alias,
type-cleanup order, group-cleanup order, and deferred-token tests.
`tools/verify_avs_descriptors.py` executes the original x86
initialization and lifecycle instructions in bounded Unicorn memory and compares
**18,244 commands**, including **768 bulk cleanup calls**, against the native
implementation. Pool sizes are 1, 2, 17,
and the original boot default 808, with deterministic interleavings of allocation,
replacement, lookup, release, stale-token access, and reuse across all nonzero
types. Another 64 deterministic scenarios mix all four groups, type and group
cleanup, retained-token lookups, and allocation after cleanup. Reserved owners
receive explicit synthetic callbacks before group-zero cleanup. The oracle
compares the complete destructor trace and verifies that callbacks run after
unlocking.

Only memory fill, diagnostic logging, locking, and a synthetic destructor are
intercepted. No Windows mutex object pool, live AVS process, hardware operation,
or network connection runs. The report is retained at
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/avs-descriptors-cleanup-report.json`.

```powershell
python tools/verify_avs_descriptors.py `
  --dll E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/libavs-win32.dll `
  --dependencies E:/MBR-2019121100/IDA/RBiOS/.tools `
  --executable <path-to-rbavsdescriptors> `
  --report E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/new-descriptor-report.json
```
