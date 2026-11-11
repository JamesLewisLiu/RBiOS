# Concrete read-only AVS mount registry

`AvsMountRegistry` connects recovered path normalization, mount indexing,
descriptor lifetime and actual default-filesystem host reads. It can mount
`fs` at `/`, `/dev/nvram`, `/dev/raw` or another virtual prefix, and then open
files through those virtual paths. A virtual mount named `/dev/nvram` does
not imply the `nvram` filesystem backend: the requested filesystem type must
be recovered independently. Unavailable types return AVS error 19.

The explicit `registerReadOnlyDefaultFilesystem()` entry point registers the
native read-only capability. It does not pretend to implement original
`addfs` validation, all 27 callbacks, filesystem init workers, or the other
five boot-time filesystem descriptors. This module alone is not a successful
AVS or game boot. No application startup stage is bypassed by this module.

## Original decisions and native lifetime

| Behavior | Evidence |
| --- | --- |
| Mount descriptor type 10, duplicate error 17, pool exhaustion error 12 | `0x1002CD00` |
| Exact mount matching with hash, depth, byte length and path | `0x1002C9A0` |
| Index bucket `depth * 8 + (hash & 7)`, newest record first | `0x1002CD00` |
| Search deepest prefix first, check slash/NUL boundary and full prefix | `0x1002C820` |
| Acquired mount references and release | `0x1002C7A0`, `0x1002C6F0` |
| Busy-unmount error 16; missing-mount error 19 | `0x1002C920` |
| Mount descriptor destructor removes the index entry and owned work | `0x1002CEF0`, `0x1002CC80` |
| Exact mount suffix `/`; root suffix whole path; other suffix after prefix | `0x1002BD00` |
| Concatenate device and suffix within 256 bytes; Windows slash conversion | `0x1002A8B0`, `0x1002A900` |

The default mount callback does not check whether its device exists. This
module likewise records a valid mount and reports a missing device when a
real file open fails. File opens acquire a mount reference; failed opens
release it. Closing a file releases its reference, and unmount refuses live
references. The recovered `AvsDescriptors` pool owns mount tokens and invokes
the cleanup callback on release. Shared native state keeps a live file valid
if the outer registry wrapper is destroyed first.

Native mutexes and host-sized objects replace global x86 pointers and linked
records. Mount publication is serialized, so partially initialized records
are never exposed to another native caller. The host-file object is an RAII
lease, not a claim that the original file-descriptor allocation API has been
restored. There is no write path in this module.

## Bounded areas

Link mounts and their substitution/loop handling remain unavailable.
Relative paths use the root current directory; the global AVS `chdir` API is
not exposed here. Host separators change only at the platform boundary.

`AvsDefaultMountOptions` restores the original table-driven grammar for the
default filesystem, including quoting, escaping, separators, missing/invalid
values, and ignored unknown keys. It preserves option callback effects before
a later parse failure. The actual inherited nvram options
`sz_block=512,nr_block=32,nr_mirror=4` are ignored when that mount uses `fs`,
matching the original. `vf` affects write flushing and has no effect on this
read-only capability. Other filesystems' option tables are not implemented.

The recovered numeric helper `0x10010770` uses wrapping 32-bit base-0
arithmetic, unlike libc `strtol`. Consequently `4294967296` evaluates to zero,
and consumed sign/prefix-only values such as quoted `+` or `0x` also yield
zero. The native parser retains these observed behaviors. Its two callbacks
receive the boolean conversion result, not an arbitrary integer's low byte.

The original low-level descriptor allocator returns zero on exhaustion, but
the mount caller checks only negative values. The native boundary returns
zero without mounting when allocation returns zero, avoiding rebinding a
reserved descriptor. This is an explicit safety boundary outside the verified
normal allocation domain. Device strings of 256 or more bytes are rejected
instead of relying on the original bounded-copy behavior.

## Validation

Standalone tests create fixtures under the supplied build directory and
exercise root/raw/nvram virtual mounts using `fs`, actual reads, duplicate and
unavailable mounts, busy unmount, failed-open reference rollback, mount-pool
exhaustion, cleanup and file lifetime beyond the wrapper. Original game files
are not changed or converted.

`verify_avs_mount_registry.py` compared **500 cases** with unchanged original
instructions for path normalization, longest-prefix lookup, suffix selection
and host-path concatenation. Coverage includes colliding `Aa`/`BB` hashes,
prefix boundaries, nested mounts and missing mounts. The original functions
execute without hooks. Mount records are synthetic, so this oracle does not
validate registration, descriptor exhaustion, host I/O or complete boot.

`verify_avs_mount_options.py` additionally compares **1,259 cases** against
the original mount callback and complete option-parser instructions. Only
scratch allocation/free are intercepted. It compares the final return and
both option bytes, including partial effects before failures. Cases cover
the actual inherited nvram options, all 256 inserted byte values, quoting,
escaping, buffer limits and numeric wraparound. The report is
`avs-mount-options-oracle.json` in the same evidence directory.

Evidence and report are under
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/`:
`official-startup-services-pseudocode.json`,
`official-path-join-pseudocode.json`,
`official-services-pseudocode.json`,
`official-services-leaves-pseudocode.json`, and
`avs-mount-registry-oracle.json`.
