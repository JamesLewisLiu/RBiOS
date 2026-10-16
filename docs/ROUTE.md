# Original route construction and redirection

Route restores source-to-target motion geometry from original prepared chart fields. It is native C++, not an x86 memory overlay.

Evidence includes decompiled 0x100A46C0, 0x100A45C0, 0x100A3270, 0x100A3380, 0x100A3210, 0x100A3070, 0x100A2F10, 0x100A2DA0 and 0x100A2C30. The game hash is fixed in [MIGRATION](MIGRATION.md).

D3DX evidence is modules/d3dx9_41.dll, SHA-256 3866ed143c8e32ba08b0de985575bccefb1498176cbfc6afb05425db5a5069b0. The independent fixture directs D3DXVec2Normalize dispatch at 0x103C722C to original scalar code 0x101D54A4.

## Interfaces

buildRoute restores 0x100A46C0, including start point 0, up to three intermediate points, destination 4, special reflection point 5 and original horizontal handling at point 6. MotionRoute is passed to MotionTimeline::initialize with original duration and intermediate count.

The stage's 0x1015AAD0 supplies side from chart+332, kind from +288, widthMode from +400 and suppressWallReflection from chart+284==1. Do not reinterpret wire bytes in this interface.

BuiltRoute.cumulativeFractions follows 0x100A45C0. Preserve zero-distance and destination branches instead of normalizing every fraction to an invented value.

initializeRoute restores standard non-linked 0x100A3270. Its config corresponds to motion+24, with pathClass +8, side +16, target +32 and direction +36. Original class dispatch and table 0x10292708 determine intermediate selectors.

configureChartRoute restores 0x100A3380 and its call into 0x100A2C30 with original ECX state. Classes differ for path kinds 7..9 and noteVariant==1 versus other variants, and for path kinds 1/2/4/5/6. previousTarget comes from motion+124/+128; the supplied note position is not a replacement.

redirectRoute restores 0x100A3210 with 0x100A3070/0x100A2F10. It updates the route and config direction for linked/JR behavior while retaining original reflection and destination branches.

normalizeRouteDirection follows scalar D3DX instruction storage: float squared sum, float square root, float reciprocal and component multiplication. It preserves the near-unit fast return and tiny-vector zero result. Alternative CPU-specific SIMD dispatch is unverified; callers may inject another normalizer.

## Validation

tools/verify_route.py passes:

| Scope | Cases |
| --- | ---: |
| Route geometry with original D3DX scalar instructions | 3,000 |
| Standard route seeds | 2,000 |
| Chart configuration | 600 |
| Linked/JR redirection | 3,000 |
| Total | 8,600 |

Report: .build/route-parity.json. Two host checks also pass.

The fixture explicitly uses x87 control word 0x027f: 53-bit precision, nearest rounding. Unicorn's default word zero implies 24-bit precision and can shift reflected positions by a pixel; it is not evidence of the original CRT environment. The live process control word has not been observed. CRT _CIsqrt is represented by x87 fsqrt.

Original D3DX dispatch alternatives, complete stage call order, linked actor side effects and device floating-point results require separate evidence. StageSession integration and its host audits are described elsewhere.
