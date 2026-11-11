# Original AVS boot configuration import

The native importer recovers the boot configuration image consumed by AVS2
2.15.8 r6631. It does not start AVS services or claim that the original bootstrap
has completed. It accepts an already-decoded `Property` named `config`, or a
null pointer for the original defaults-only path.

Source identity: `modules/libavs-win32.dll`, SHA-256
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.
The implementation uses the 48 records at `0x10076C20` and their `0xFF`
terminator. Every boot record has the optional/default flag set. The native
table preserves record order, paths, offsets, widths and defaults, including
the pointed 64-bit affinity mask `0xFFFFFFFFFFFFFFFF` and descriptor count 808.
This is a byte image; it does not depend on ARM64 struct alignment or pointer
size.

## Recovered behavior

| Original address | Recovered behavior |
| --- | --- |
| `0x10013A70` | Require the `config` node name, zero all 2528 image bytes, invoke import, then dump the resulting map. |
| `0x10006C80` / `0x10006C8C` | Traverse the psmap in order; resolve each path; use its default if the node is absent. |
| `0x10006E20` | Store immediate scalar defaults, pointed 64-bit defaults, or bounded string defaults. |
| `0x10007160` | Translate handle types through `0x10067040`, call `property_node_read`, require scalar byte counts to match. |
| `0x1000D490` / `0x10034000` | Reject mismatched base node types. The array bit is ignored by the type comparison. |
| `0x100340CC` | Reject a byte count exceeding destination capacity, then copy the recorded bytes without string rescanning. |
| `0x10006BD0` | Dump/log the map; this does not correct values. Product-mode correction is separately recovered at `0x10013B30`. |

Psmap string handle 10 maps to property node type 11; boolean handle 50 maps to
node type 52. Numeric handle and node types used by this table match directly.
`Property` keeps binary numeric values in big-endian order, so native import
writes their little-endian representation into the original image. Strings keep
their original bytes and encoding. No asset conversion or resource output occurs.

The original reader copies string byte counts verbatim, including embedded
terminators. It does not add a terminator or truncate an oversized value. The
native importer preserves that behavior; downstream readers must remain bounded.
The mount-table filename field has capacity **256**, at offset **1731**. Root,
NVRAM and raw device/option fields have capacity **257**.

## Verification

`AvsConfigImportTests.cpp` checks the default image, padding, scalar byte order,
overrides, root rejection, base-type/size rejection, array-bit behavior and raw
string copies. `verify_avs_config_import.py` loads the hash-bound original DLL
in Unicorn and compares every byte of the 2528-byte native result against the
original import instructions. It includes all defaults, four boundary values
for each of the 48 mapped fields, array flags, raw string edge cases, and a
typed fixture read from the unchanged original `prop/avs-config.xml`, 48 same-base array-flag cases and 39 raw string
boundaries (empty, embedded terminator and full capacity without a terminator).
All **281 cases passed**, comparing every configuration byte and lookup order.

The defaults-only oracle executes original code without function replacements.
Override cases replace only path lookup (`property_search`) with a fixture
callback. Synthetic original node layouts feed the original `property_node_read`
and copy routines. The lookup order is checked against the original table.
This distinguishes verified import behavior from the unported property allocator
and path resolver.

Example (put output outside original assets):

```text
python tools/verify_avs_config_import.py --game-root E:/MBR-2019121100/IDA --dependencies E:/MBR-2019121100/IDA/RBiOS/.tools --executable E:/MBR-2019121100/IDA/RBiOS/.build/active/rbavsconfigimport.exe --report E:/MBR-2019121100/IDA/RBiOS/.build/active/avs-config-import-oracle.json
```

## Remaining boundary

This is the boot table subset, not a general implementation of all psmap handle
types. Original XML parsing, property allocation and AVS path-query semantics
are not ported here. The Python XML reader only prepares an offline test fixture;
it is not the application parser. Logging is not replayed by this API. Native
exceptions expose rejected input instead of returning a partially valid image.
Real AVS full boot, iPad host services, mounts, TEST/game initialization and
network operation require their own verification. A configuration import pass
does not establish any of those states.
