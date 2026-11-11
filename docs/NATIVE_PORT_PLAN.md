# Complete original native port: accepted execution plan

Accepted 2026-11-11, based on 377228b. Completion requires the real AVS2 -> EA3 -> game entry -> loading -> TEST/game scene chain, with no successful placeholders on required paths. The current Apple application remains a component harness.

## Required order and remaining work

1. Recover AVS properties (XML and binary), query/allocation/serialization, descriptors, memory, threads, synchronization, clocks, filesystem services and full startup/cleanup. Existing dispatch and configuration code do not implement all these services.
2. Recover EA3 finalizers, facility/service lifecycle, protocol/transport and errors; connect original game entry, cache, graphics, fonts, sound, devices and process owners.
3. Restore complete TEST controllers and effects, attract/title, identity/guest, selection, loading, stage, end presentation, result and return. Recovered caller state must supply CPU, RNG, rules, eligibility and account parameters.
4. Complete AFP scripts, dynamic bindings, geometry/masks/fonts, sound ownership and iPad adapters. Keep original update semantics; fixed 768x1360 coordinates and 50%-100% scale; request up to 120 Hz presentation and measure real device frame times.

## Storage and networking decisions

The user explicitly permits cabinet configuration writes in the selected original resource root's prop/dev directories. Back up the original version to a separate application-owned directory before first replacement. Preserve original serialized bytes, paths and triggers. A write failure must be visible; never silently redirect writes. Other assets remain byte-identical and are decoded only in memory.

Do not add local score/profile/history persistence or a deferred upload queue. Configuration storage is a platform boundary, not permission to save arbitrary player data. Only recovered cabinet configuration call sites may invoke it.

Default remains offline. Remote mode targets 10.9.0.19:18083 after original protocol recovery and offline tests. The accepted plan permits service discovery/compatibility checks. No test identity has been selected: login, registration and score submission remain pending explicit test identity; do not generate or guess one. Embedded server implementation awaits source.

## Verification and delivery

Keep original function addresses and evidence; compare instructions, then subsystem traces, then original scene behavior. Test missing/corrupt config/resources, allocation/mount failures, cancellation and partial cleanup. Keep all reference-runtime configuration in isolated copies. Verify operator settings/restart and full game round trips on M1 iPad Pro/iPadOS14.6, with iPadOS14.0 minimum.

Code: G:/vibe/RBiOS. Analysis, tool caches and fixtures: E:/MBR-2019121100/IDA. Leave the Spice repository unchanged. Use Official IDA MCP for decompilation. Authored documentation, code comments and app strings remain English.

Commit verified increments and follow macos-15 Actions directly to completion; create no recurring task. Package matching IPA/TIPA. Report code coverage, instruction checks, Apple builds, device verification and server verification separately. Do not label an increment as completion of the full port.
