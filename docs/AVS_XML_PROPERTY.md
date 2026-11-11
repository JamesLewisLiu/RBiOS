# Original XML property loading for boot configuration

`AvsXmlPropertyBuilder` converts XML element, attribute and text events into the
existing native `PropertyDocument`. It supports every node in the supplied
`prop/avs-config.xml`. XML syntax and character decoding are explicit platform
boundaries: the Apple adapter supplies events and restores text/attribute bytes
to the declared document encoding. The builder does not translate resource files
or write converted XML to disk.

Source DLL SHA-256:
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.
Original paths were followed through Official IDA MCP:

| Address | Behavior |
| --- | --- |
| `0x1000B080` / `0x1000B180` | Property creation and original input-reader entry. |
| `0x10035390` / `0x10052C90` | XML callback setup and syntax parser. |
| `0x10035FD0` | Allocate/insert nodes; duplicate sibling handling follows create flag `0x10`. |
| `0x10035B50` / `0x10035740` | Reserved metadata and ordinary attributes. |
| `0x10035F90` / `0x10055220` | Select typed content handlers from `0x10079720`. |
| `0x10054C50` | Untyped whitespace stays void; nonwhitespace promotes to string. |
| `0x10055100` / `0x10054DF0` | Allocate numeric scalar/array storage and split numeric tokens. |
| `0x10054A30`, `0x10054810`, `0x100545F0`, `0x100543B0` | Unsigned 8/16/32-bit and boolean conversion. |

## Implemented behavior

- Void/untyped nodes, inferred strings, explicit strings, `u8`, `u16`, `u32`
  and numeric booleans. Boolean values must evaluate to 0 or 1.
- Original type aliases and integer conversion quirks from `AvsXmlMetadata`.
- `__count` numeric arrays, zero-filled missing values, and rejection of excess
  values. Empty numeric scalars contain zero. Explicit strings reject counts.
- Ordinary attributes retain original bytes plus the original NUL terminator.
  Duplicate attributes and unknown reserved metadata fail.
- Strings preserve surrounding whitespace and original bytes. Empty explicit
  strings contain one NUL; empty or whitespace-only untyped nodes remain void.
- Duplicate sibling nodes are controlled by the constructor flag. The copied
  original launcher uses property-create flags 23 (`0x17`), which allow duplicates.
- A completed document is consumed once. Broken event order and unsupported
  operations throw errors; an incomplete document cannot be returned as valid.

The actual configuration declares `Shift-JIS`. A full original-reader probe
confirms Japanese text remains CP932 bytes in property storage. The Apple token
adapter therefore must round-trip decoded text back to CP932, with no lossy
replacement. Native numeric property values use the existing big-endian byte
representation; import subsequently restores original little-endian image bytes.

## Verification

`verify_avs_xml_property.py` executes the hash-bound original `property_create`,
`property_insert_read`, XML parser, typed conversion, tree allocation and
`property_search` in Unicorn. Only the input-reader callback supplies file bytes;
no parsing, allocation or conversion function is replaced. Synthetic documents
and the unchanged original configuration are compared with the native builder.
The oracle compares complete trees, names, attributes, type/array flags, numeric
bytes, original CP932 bytes and acceptance/rejection. Native token fixtures use
ElementTree solely as the explicit XML syntax boundary and split text events to
test callback chunk independence.

The initial run passed 56 document comparisons: 42 complete decoded trees and
14 rejected inputs. Native unit tests also exercise XML events through boot
configuration import.

## Explicit limits

This module does not reproduce the entire original XML grammar. Platform XML
parsers may differ on malformed syntax, declarations, entities and namespaces;
those differences must not be reported as original-parser equivalence.
Explicit `__size`, signed/64-bit/floating-point/vector/binary content, mixed text
and children, and names outside the recovered ASCII subset remain unsupported.
Numeric/text allocations are capped at 16 MiB as a native resource limit.
Exceptions preserve failure rather than fabricating a usable property. Original
numeric error codes and allocator side effects are not exposed by this API.
Serialization and cabinet XML writes are not implemented here.
