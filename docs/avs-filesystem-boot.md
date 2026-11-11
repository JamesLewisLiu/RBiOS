# Recovered AVS filesystem boot phase

This module restores the control flow of `libavs-win32.dll` at `0x10013E00`,
including helpers `0x10013FA0` and `0x10014010`. It does not provide filesystem
implementations or establish that AVS or REFLEC BEAT can boot. The application
must not substitute successful callbacks for the missing services.

The source DLL is AVS2 2.15.8 r6631, SHA-256
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.
Official IDA MCP pseudocode was checked against the original x86 instructions.
Analysis evidence is retained under
`E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/` in
`official-import-mount-pseudocode.json`, `avs-boot-table.json` and
`avs-mount-oracle-v2.json`.

## Original sequence

The first byte of the root device at configuration offset 155 must be nonzero.
Failure enters the original fatal logger before any filesystem registration.
The registration table at `0x10068ABC` contains six name/factory pairs:

| Name | Original factory |
| --- | --- |
| default | `0x10029510` |
| nvram | `0x1001DAE0` |
| imagefs | `0x10023270` |
| mirrorfs | `0x1002B270` |
| cryptfs | `0x1003B550` |
| nvram2 | `0x1003CA30` |

Each factory's result is passed to `XCd229cc00012d` at `0x10029080`.
There is no descriptor-null check. The helper skips a null name; otherwise,
only a signed result greater than zero succeeds. Failure stops the caller at
its fatal-log branch, without undoing earlier registrations or mounts.

Next, the caller passes these configuration fields to `XCd229cc0000ce` at
`0x1001B2D0` in path, device, filesystem type, options order:

| Mount path | Type offset | Device offset | Options offset |
| --- | --- | --- | --- |
| `/` | 138 | 155 | 412 |
| `/dev/nvram` | 669 | 686 | 943 |
| `/dev/raw` | 1200 | 1217 | 1474 |

The field widths are 17, 257 and 257 bytes, respectively. No path normalization,
option interpretation or numeric mount flags are introduced. In the x86
usercall helper, the options pointer is the fourth incoming stack slot; the
three preceding slots are unused. This detail prevents confusing IDA's extra
prototype parameters with flags or omitted mount parameters.

The helper skips a null path or an empty device. Otherwise, a signed mount
result greater than zero succeeds. Empty filesystem/options strings are passed
through. Failure enters the fatal branch before attempting the next mount.

The original property-map table confirms a 256-byte mount-table field; the
otherwise unused following byte is not part of that field. If the mount-table
path at offset 1731 is empty, the function returns
zero. Otherwise it opens that path with `XCd229cc0000cb` at `0x1001B020`. A
nonpositive handle is fatal. A positive handle is passed with selection `ALL`
to `XCd229cc000047` at `0x1001AF50`. The apply return value is ignored, and the
open handle is returned. The caller does not close this handle in this function.

## Native boundary and verification

`runAvsFilesystemBoot` takes the existing 2528-byte configuration image and a
mandatory `AvsFilesystemBackend`. The abstract interface has no default
implementation. Descriptor tokens are host-sized, and original addresses are
provenance metadata only. The native adaptation replaces fatal termination
with `AvsFilesystemBootError`. It additionally rejects unterminated strings
within their configuration fields instead of allowing an out-of-bounds read.
Formatted logging is not implemented in this bounded module.

`AvsMountTests` checks the complete ordered boot-phase trace, each failure
stage and its stopping point, optional-device skips, mount-table absence,
ignored apply failures, helper skip rules, zero descriptor forwarding, signed
result boundaries, and rejection of malformed fields. It verifies that a
255-character mount-table path is accepted but a field with 256 nonzero bytes
is rejected, even when the following byte is zero. `verify_avs_mount.py`
compares 960 cases against the unchanged original caller/helper instructions
using Unicorn. It checks the ordered callback trace and arguments, optional
devices, empty root device, failures at successive operations, and returned
mount-table handle. Factory bodies, actual filesystem exports, logging and
fatal termination are intercepted. These results verify dispatch and failure
semantics; they do not verify real file access, mount behavior or game boot.

Example local verification, with build and report outputs under the analysis
directory:

```powershell
python G:/vibe/RBiOS/tools/verify_avs_mount.py `
  --game-root E:/MBR-2019121100/IDA `
  --dependencies E:/MBR-2019121100/IDA/RBiOS/.tools `
  --executable E:/MBR-2019121100/IDA/RBiOS/.build/avs_mount_tests.exe `
  --report E:/MBR-2019121100/IDA/RBiOS/.build/original-boot-review/avs-mount-oracle-new.json
```
