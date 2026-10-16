# Native code recovery, iPad adapters and networking

This is the selected implementation direction, recorded 2026-11-04. Later numbered revisions override specific build, persistence and delivery details.

- Restore C++ from the matching binary, disassembly and existing Hex-Rays text. Record addresses, types, layout evidence and missing behavior. Cover charts, motion, judgment, score, CPU, pages and required AFP/AVS logic.
- Target iPadOS 14+, primarily M1/iPadOS 14.6. Use Swift/UIKit shell, Objective-C++ bridging and native audio/graphics.
- Import original RBFF, IFS/KBinXML/AVSLZ, 2dx/SD9 and fonts unchanged. Decode only at runtime and release assets on demand.
- Keep logical/render size 768x1360. Use fixed SKRenderer/MetalKit drawableSize; fit safely and center at 50%-100%, default 100%. Inverse-map touch without Retina multiplication. Change scale only in menus/pause; cancel contacts on layout/lifecycle changes.
- Restore discovery, card/identity, profile, score and information modules. Separate protocol, state and transport. Default offline; test synthetic messages/mocks only, without real server connections or offline-score upload.
- ServerMode reserves independent and in-app services. Embedded migration awaits existing server source and stays unavailable until integrated. No backend changes during an active session; isolate account/cache state.
- Validate chart -> object -> judgment -> score before expanding scenes/protocol. Missing behavior fails explicitly. Report code recovery, build, offline tests, device play and live-server compatibility separately.
- Deliver Xcode project, tests, scripts, import and migration guides. The original build entry reserved self-hosted macOS/Xcode 15.4 rather than relying on retired hosted macos-14.
- Restrict all local code, caches and evidence to E:\MBR-2019121100\IDA. Do not include original game files in Git. Pushing was initially user-managed.

Historical plans are conversation archives, not original game source or completed implementation claims.
