# Recovered protocol fields and offline boundaries

All addresses refer to the fixed original DLL at base 0x10000000 and the existing decompilation. These interfaces are restored from this version, not inferred from another REFLEC release.

## Original requests and callbacks

| Address | Decompilation line | Interface / observation |
| --- | --- | --- |
| 0x1009CFE0 | about 124584 | PlayerReadRequest::prepare and original string/card mapping |
| 0x1009CEE0 | about 124559 | PlayerReadRequest::fields in original order and types |
| 0x1009C490 | initializer | read_score rid storage with 17-byte capacity |
| 0x1009C430 | about 124320 | playerReadScoreFields, containing rid |
| 0x1009D320 | about 124699 | rb6_player_read method |
| read_score registration | about 314532 | rb6_player_read_score method |
| 0x1009EB80 | about 125384 | Clear +520/+524/+528/+532; set pending +540 |
| 0x1009F1B0 | about 125618 | XrpcCallbackState::originalStatus |

0x1009CEE0 calls AVS symbol XCd229cc00002c through IAT 0x10189744. Original field insertion order and numeric types are retained.

| Order | Name | AVS type | Original storage |
| --- | --- | --- | --- |
| 1 | rid | 11 / str | Prepared +550, 17-byte buffer |
| 2 | lid | 11 / str | Prepared +567, 9-byte buffer |
| 3 | ver | 4 / s16 | Prepared +548 |
| 4 | card_id | 11 / str | Prepared +576, 17-byte buffer |
| 5 | card_type | 4 / s16 | Prepared +594; original mapping 1->1, 2->2, otherwise 0 |

The callback status reads the object at +520. Pending returns 3. Otherwise original +8 selects the table below. Where that code is absent, +0==0 returns 0, +12 selects 1, and the remaining branch returns 4.

| Original +8 code | Result |
| --- | --- |
| 100, 101 | 2 |
| 102 | 14 |
| 109 | 6 |
| 110 | 7 |
| 111 | 15 |
| 112 | 9 |
| 113 | 10 |
| 114 | 11 |
| 115 | 13 |
| 116 | 5 |
| 120 | 12 |
| 123 | 8 |
| Other nonzero code | 1 |

## Validation

tools/verify_protocol.py executes original prepare/field builders for eight cases, including card-type mapping and signed version values. AVS allocation/string copy is an explicit fixture. Independent kbinxml 2.2 output is compared with native KBinXML bytes.

Original 0x1009C490/0x1009C430 validates the score-read rid structure. Original 0x1009F1B0 is compared against 1,152 callback states without imported calls. In-capacity ASCII fixture strings are covered; original truncation semantics are not recovered.

The general property suite compares 52 synthetic typed trees with kbinxml 2.2, including CP932/UTF-8, compressed/raw names and scalar/array values. Non-ASCII fixture values are represented by source Unicode escapes; their runtime bytes are unchanged.

## Transport and unresolved behavior

NetworkTransport is injectable. Modes separate offline play, an independent server and an embedded-service interface. Switching is disallowed with an active session and account/cache state is isolated. Embedded service remains unavailable without server source. Offline scores are not queued for later upload.

The Swift URLSession adapter is dormant and defaults to disabled. Its URLProtocol tests are written but have not run on Apple. It is not yet connected to the complete C++ business client.

Original 0x1009EAF0 calls EA3 symbol XE592acd000004. Full EA3 envelope, encoding/compression negotiation, HTTP rules and response business parsing remain unfinished. services.rb4get registration at 0x1009B380 uses a template at 0x10292D40, but 0x1009B280 is a direct return in this build. Card methods inquire/getrefid/authpass/bindmodel/getdatalist are known names, not recovered wire schemas.

rb6_player_read response 0x1009D090 and read_score response 0x1009C3F0 route pdata into profile structures. Complete field parsing, account lifecycle, writes, score submission and discovery are not implemented by the field-builder tests.

All validation is offline. No server probe, login or score upload has occurred, and mock compatibility is not live compatibility.
