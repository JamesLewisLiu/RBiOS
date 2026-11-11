# Original AVS path normalization and mount prerequisites

`AvsPath` restores the actual path-processing algorithm at `0x1002C110` in
AVS2 2.15.8 r6631. It computes canonical virtual paths and the component hashes
used by the mount index. It performs no host filesystem operations and is not
a successful-mount substitute.

Evidence comes from Official IDA MCP output
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/official-services-leaves-pseudocode.json`
and direct inspection of the original instructions and byte-permission table
at `0x1006C5B8`. Source DLL SHA-256:
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.

## Restored behavior

An absolute input ignores the current directory. Otherwise a nonempty current
directory is consumed first, followed by an inserted slash and then the input.
Repeated slashes and single-dot components disappear. Parent components remove
the previous component and restore its preceding hash; traversal above the root
stays at the root. Root normalizes to `/`. Embedded NUL bytes retain the original
C-string termination semantics. Backslashes are invalid, not separators.

The original eight-word bitset controls accepted bytes. No locale, Unicode
conversion, case folding or host path normalization is substituted. Each
component contributes its bytes to the wrapping 32-bit recurrence
`hash = hash * 31 + byte`; slashes do not contribute. Component hashes therefore
must be used with path equality, not as unique identities. The result contains
all prefix hashes, including the root hash zero and the full-path hash.

The positive return value packs the component count in the high 16 bits and
the normalized byte length in the low 16 bits. The original limit is 64
components. Capacity includes the trailing NUL. Invalid bytes return
`0x80070016`, while capacity/component exhaustion returns `0x80070024`.
The order of byte validation and capacity checks matches the instructions.

The native boundary additionally rejects capacities below two or above 65535.
Original callers use 128 or 256 bytes; the original root case could overrun
a caller-provided buffer below two bytes. Native error results contain no
partial output, because original partial buffers have no successful meaning.

## Verification

The standalone `AvsPathTests` runs without proprietary files and covers path
normalization, parent/hash rollback, absolute paths, invalid bytes, embedded
NULs, capacity edges and the 64-component limit.

`verify_avs_path.py` ran **3,401 cases** against the unchanged original
`0x1002C110` instructions using Unicorn. There are no hooks, logging stubs or
substituted calls in that oracle. It compares signed status and, on success,
the full normalized path, byte length, component count and every prefix hash.
Coverage includes all 256 input-byte values in four positions, capacity
boundaries, deep paths and deterministic randomized combinations. Error buffer
contents are not compared. Report:
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/avs-path-oracle.json`.

## Remaining mount service

The public mount function `0x1001B2D0` validates its arguments, invokes path
processing through `0x1002BEF0`, and dispatches type `link` through
`0x1002C4F0`/`0x1002CB00`; other types enter `0x1002CD00`.

The real mount service still needs the registered descriptor lookup at
`0x10043160`, handle allocation at `0x100148F0`, existing-mount lookup at
`0x1002C9A0`, mount-index allocation/insertion, synchronization, filesystem
callback setup at `0x1002CF20`, rollback and destruction. Its index uses depth
and the low three hash bits, then verifies the path. These services are not
implemented by `AvsPath`.

The default filesystem factory at `0x10029510` returns descriptor `0x10077F0C`.
Its actual descriptor name is `fs`; `default` is the boot registration label.
The descriptor points to the callback table at `0x10077EA0`. Its mount callback
at `0x1002A960` initializes option bytes to `[0, 1]` and parses `posix`/`vf`
options through the original option parser. It returns zero on success. This
callback is not itself a native directory mount or a positive AVS mount handle.
The public mount's positive handle arises from the surrounding service.

No host-directory adapter is introduced at this stage: mapping virtual roots
to arbitrary local paths before recovering descriptor semantics would change
the original behavior. No original resources have been changed or converted.
