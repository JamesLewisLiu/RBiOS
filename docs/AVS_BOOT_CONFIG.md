# AVS2 product-mode configuration recovery

This module restores one actual AVS2 boot transformation. It does not implement AVS boot, property import, descriptors, threads, filesystems, network initialization or game loading.

Target: `modules/libavs-win32.dll`, AVS2 2.15.8 r6631 win32, SHA-256 `d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`. Fresh Official IDA MCP pseudocode for export `XCd229cc0000aa` at `0x100138C0` identifies the product-mode branch calling `0x10013B30` after configuration import. Original instructions corroborate field offsets, widths, branch order and log arguments.

`AvsBootConfigImage` is the original 2,528-byte imported configuration representation. It is a byte image, not an ARM64 pointer-bearing object layout. When product mode is selected, `applyAvsProductMode` clears only these fields, emitting a correction record only for each initially nonzero field:

| Offset | Width | Original configuration path |
| --- | --- | --- |
| 1 | 1 byte | log/netsci/enable |
| 118 | 2 bytes | thread/mutex_delay |
| 2268 | 4 bytes | time/debug_local_offset |

The correction order is the table order. The first two old values are unsigned; the last is signed 32-bit. All other bytes remain unchanged. With product mode disabled, the caller does not execute this transformation. The result consists of correction records, never a boot-success flag. Integration must supply the product-mode value from the recovered property import, not assume it.

## Validation

Native checks cover all eight field combinations, disabled mode, original signed/unsigned log values, unchanged unrelated bytes and idempotence. `verify_avs_boot_config.py` executes the original `0x10013B30` x86 instructions and compares all 2,528 bytes plus ordered log fields/old values against native output for 256 cases. Only formatted logging at `0x1000A980` is stubbed. The original module is not loaded into a process and no network operation occurs.

The full AVS caller and configuration parser are not executed by this bounded oracle. This recovery cannot be used to claim that AVS2 or the original game boots on iPad.
