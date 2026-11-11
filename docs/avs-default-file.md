# Default filesystem read-only host callbacks

`AvsDefaultFile` provides actual host file access for the recovered default
filesystem callback subset. Windows uses the original host APIs; Apple uses
POSIX file descriptors and native file metadata. It takes an already-resolved
host device path. It does not invent a mount registry or declare AVS booted.

Official IDA MCP evidence:
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/official-fs-host-pseudocode.json`.
All addresses below refer to AVS2 2.15.8 r6631 `libavs-win32.dll`, SHA-256
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.

| Native method | Original callback | Restored behavior |
| --- | --- | --- |
| open | `0x1002A560` | Access bits `flags & 3`, read-only existing-file open, optional initial EOF seek for `0x40`, case verification |
| read | `0x1002A1F0` | One host read, actual byte count, zero at EOF |
| readv | `0x1002A130` | Sequential reads, stop at first short read, return accumulated count; an error discards prior count |
| seek | `0x1002A240` | 64-bit offset/position, origins 0/1/2, invalid-origin error |
| close | `0x1002A2E0` | Read-only close without write flush or directory flush |
| stat | `0x10029EA0` via `0x10029FC0` | Birth/write/access milliseconds, link count, 64-bit size, directory/file type, original fixed mode 0644 |

The original Win32 error mapping table at `0x10077680` is retained in
`AvsFileWindowsErrors.inc`; the file callback path uses the non-socket branch
of `0x100247D0`. Unknown errors retain the original 4095 mapping. POSIX common
file errors are explicitly mapped to AVS values. Unmapped POSIX errors also
report 4095 and are not silently treated as success.

Windows preserves original permission-derived share flags and case-sensitive
name checking. Apple verifies directory-entry spelling before opening when
the `posix` option is enabled, including on case-insensitive volumes. POSIX
has no direct equivalent to Windows share-mode flags. The wrapper owns its
handle and closes it on destruction; repeated opens/closes are explicitly
rejected. Byte counts above `INT32_MAX`, null nonempty buffers and embedded
NUL host paths are rejected at the native boundary. The original low-level
callbacks did not validate all of these inputs.

The stat layout is represented by typed values instead of a packed x86
structure. Windows FILETIME is converted from 100 ns ticks to Unix
milliseconds; Apple uses native birth/write/access timestamps. Other POSIX
platforms currently report stat unavailable because creation-time support has
not been established. Read/write/create/truncate modes are explicitly
unsupported by this module. It never creates or modifies a file through its
production API.

Instruction review at `0x1002A635` confirms that the original read-access
branch selects `OPEN_EXISTING` before processing create `0x10`, truncate
`0x20` and exclusive `0x80`; those bits affect the original write branches.
This adapter preserves that behavior: when the low access bits equal one,
other bits are ignored except the active initial EOF seek bit `0x40`.
Standalone tests cover individual and combined ignored flags, including high
bits: existing files retain their length and bytes, missing files are not
created, and `0x40` still seeks to EOF. Write access remains unsupported.

## Verification

Standalone tests open a generated fixture and exercise reads, EOF, short
vector reads, preserved unused buffers, errors after earlier successful reads,
64-bit seeks, metadata, initial append positioning, close and missing files.
They verify the fixture bytes remain unchanged. Local fixture creation and
cleanup occurred only under `E:/MBR-2019121100/IDA/RBiOS/.build`.

`verify_avs_file_read.py` compared **400 cases** of original `0x1002A130`
instructions against the native adapter reading actual generated files. It
compares return counts, final positions and complete output buffers. Original
`ReadFile` is intercepted with an in-memory byte source; the native side uses
real host I/O. This oracle covers successful/short/EOF reads, not host error
mapping, open/close/stat or full filesystem mounting. Report:
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/avs-file-read-oracle.json`.

Windows host tests passed. The Apple branch still needs the macOS CI build
and device verification. The app must not wire this adapter as a substitute
for the missing mount registry, filesystem descriptor lifecycle, link-mount
resolution or original game boot.
