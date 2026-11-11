# Original AVS XML metadata conversion

`AvsXmlMetadata` implements the original `__type`, `__size` and `__count`
value helpers. It is not an XML parser, content converter or serializer.

Source identity: `libavs-win32.dll`, SHA-256
`d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91`.
Fresh Official IDA MCP evidence establishes the XML callback chain:

| Address | Role |
| --- | --- |
| `0x10035390` | Configure XML callbacks and invoke parser `0x10052C90`. |
| `0x10035B50` | Recognize `__type`, `__size`, `__count`; reject other reserved names. |
| `0x10035740` | Convert metadata values; ordinary attributes follow a separate byte-copy branch. |
| `0x1002D690` / `0x10024E30` | Search the 65-entry type-alias table at `0x10078080`; return zero for no match. |
| `0x10024AF0` | Integer conversion; the XML callback supplies unsigned bounds 0 through UINT32_MAX. |

All 65 aliases are retained, including `str`/`string`, `bin`/`binary`,
`b`/`bool`, and original vector aliases. Matching is case-sensitive and does
not trim whitespace. A type token does not encode the separate `__count`
array state. Recognition of a type does not claim that the native content
decoder currently implements that type.

The count helper preserves observed original behavior rather than using a
standard-library integer replacement: leading whitespace is skipped, plus
signs fail, minus signs are accepted only for zero, and radix selection follows
leading `0` / `0x`. The original digit check accepts a digit equal to the radix,
so `08` evaluates to 8 and decimal `1A` evaluates to 20. Parsing stops at the
first whitespace after the number and does not validate subsequent text.
Direct zero returns status 1; other successes return 0; failures return -1.
Both zero and positive status mean success to the original XML callback.

`verify_avs_xml_metadata.py` compares all aliases, case/whitespace variants,
numeric boundaries and deterministic generated inputs against the original
instructions. The original helpers execute without replaced functions. The
native result preserves the helper status, while `value` is meaningful only
on success. Original failure writes are not exposed as a usable result.

The next unresolved XML entry is content handling behind `0x10035DB0` /
`0x10035DE0`, with the parser at `0x10052C90`. Entity decoding, declaration
encoding, node creation, array state, ordinary attribute construction, complete
syntax failures and serialization still require recovery. No caller should use
this module as evidence that XML documents can already be loaded or saved.
