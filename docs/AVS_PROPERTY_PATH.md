# Original AVS property path lookup

`AvsPropertyPath` recovers a bounded plain-name subset of AVS2 property lookup.
It operates on the existing `Property` tree without changing its representation.
It does not create property documents or parse XML.

Source: `libavs-win32.dll`, SHA-256
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.
Fresh Official IDA MCP analysis covers `0x1000CC40` (export wrapper),
`0x10037B00` (lookup dispatch), `0x100575D0` (path parser), `0x1003331C`
(ordered child lookup), `0x100334AC` (attribute lookup), `0x100098D0`
(exact name equality), and `0x1000A470` (plain-name construction).

## Recovered rules

- Empty paths retain the supplied context. Leading, repeated and trailing `/`
  characters are skipped. A leading `/` does not reset a supplied node to a
  document root. The caller must choose the original lookup context.
- `item` and `item#0` select the first matching child. `item#1` selects the
  second. Nonmatching siblings do not count.
- `item#-1` selects the last matching child; `item#-2` selects the preceding
  match. This is the original reverse traversal, not a generated index rule.
- Index numbers use base-zero integer parsing: `#017` is octal, `#0x10` is
  hexadecimal, and `#08` is invalid. Missing digits and trailing nonseparator
  characters fail.
- Attribute syntax is `id@item/path`. It returns `id` from the selected node.
  `id@` selects an attribute on the supplied context. Only one attribute prefix
  is allowed. Attribute names beginning `__` are rejected by the original parser.
- Name matching is exact and case-sensitive. Missing nodes or attributes return
  an empty reference; rejected or unsupported syntax has a distinct error.

The public result contains either a node, or its owner and an attribute byte
array. Original attribute nodes use a different layout; this adapts that result
to the existing native tree without interpreting or converting attribute bytes.

## Verification and boundary

The oracle runs the hash-bound original x86 instructions in Unicorn. Parser
cases compare ordered name, selector and attribute-context callbacks plus error
categories. Full lookup cases use a synthetic original node arena and execute
the original export, parser, exact-name comparison and forward/reverse traversal.
Only error reporting is replaced in those lookup cases. Unit tests additionally
check native references and explicit unsupported operations.

The supported name alphabet is deliberately bounded to ASCII letters, digits,
underscore and colon, with at most 4096 bytes. Other names, including `.` and
`..`, are reported as unsupported; they are not assigned filesystem semantics.
Embedded NULs and indices outside the signed 32-bit range are also explicitly
unsupported. The complete name-validation table, packed-name mode, original
allocator, hidden document root, diagnostics and mutation operations remain
outside this module. Native errors do not reproduce the original error-record
storage or partial callback side effects. Boot configuration paths fall within
the verified subset.

Run `tools/verify_avs_property_path.py` with `--game-root`, `--dependencies`,
`--executable` and a new `--report` path under the analysis workspace. Original
assets are read only.
