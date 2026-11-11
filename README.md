# RBiOS native migration

> **Recovery status:** The current Apple application is a component-validation harness, not the original game runtime. Its native menu flow is rejected as a migration implementation. Actual AVS2/EA3 boot, game loading, scene ownership and TEST dispatch must be recovered before this can be called a native port of the original game. See [execution audit](docs/FIDELITY_AUDIT.md) and [original boot chain](docs/ORIGINAL_BOOT_CHAIN.md).

Native C++ restoration of REFLEC BEAT MBR:J:A:A:2019121100, with an iPad application targeting iPadOS 14.0 and later. The primary device is an M1 iPad Pro running iPadOS 14.6.

**This is a partial migration.** Offline selection, stage execution, the CPU opponent and session-result code are connected. Windows audits, the macos-15 arm64 archive and 21 Apple simulator tests pass. User-provided iPad screenshots show a completed stage after the viewport correction. Device timing and performance remain unmeasured. Original page scenes, some effects, hit sounds and the complete online client remain unfinished.

## Documentation

Start with the [documentation index](docs/README.md), [current status](docs/STATUS.md), [build instructions](docs/BUILD.md) and [validation record](docs/VALIDATION.md). Original addresses, evidence and limits are recorded in the [migration map](docs/MIGRATION.md).

## Implemented code

- Original chart preprocessing, destination assignment, routes, note state transitions, touch association, CPU decisions, judgment, scoring and stage initialization.
- A 60 Hz C++ stage with original pool ordering and lifecycle callbacks. The Apple adapter uses the audio sample clock and timestamped touch snapshots.
- Runtime readers for original PLY, IFS/AVSLZ, textures, AFP/BSI/GE2D, SD9 and single-entry 2dx audio. Supported animation uses original images, matrices, labels and movie clocks.
- Original stage-frame panels, difficulty/round labels, player/orb graphics and bottom-panel clipping, with independent child animation clocks. Account text and portrait bindings remain unfinished; see [STAGE_FRAME](docs/STAGE_FRAME.md).
- Fixed 768 x 1360 rendering with centered safe-area fitting and a configurable 50%-100% display size. Touch coordinates use the inverse display transform.
- Pause, cancellation and background handling, a CPU opponent, and results held only in the current session.
- Offline protocol tests, injectable transports and dormant server configuration. No server is contacted automatically.

Scores, profiles and play history are not persisted. There is no deferred upload queue. Display preferences, the resource-directory bookmark and future server configuration are application settings only.

## Build

On a Mac with Xcode 16.4 installed:

~~~sh
DEVELOPER_DIR=/Applications/Xcode_16.4.app/Contents/Developer bash tools/build_ios.sh
~~~

The script archives an unsigned arm64 application, checks its minimum OS metadata and packages .build/RBiOS-unsigned.ipa and an identical .tipa for TrollStore sharing only if validation succeeds. GitHub Actions uses macos-15; see [BUILD](docs/BUILD.md) for configuration and diagnostics. A local commit does not run Actions; the workflow starts after a GitHub push.

Portable tests require no game files, Apple SDK or server:

~~~sh
cmake -S . -B .build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build .build/host --parallel 3
ctest --test-dir .build/host --output-on-failure
python3 -B -m unittest discover -s tests -p 'test_*.py'
python3 -B tools/check_repository_text.py
~~~

The prepared Windows toolchain can run python -B tools/build_host.py. Optional analysis dependencies are listed in [requirements-analysis.txt](tools/requirements-analysis.txt); keep installations and caches inside .tools and .cache.

## Original resources

This repository does not include the original DLL, game files or decompilation output. Keep the original data directory and modules/reflecbeat.dll together, then select their parent directory in the app. Files retain their original names, formats and bytes. Images and audio are decoded only in memory; no converted PNG, WAV or replacement resource bundle is generated.

The DLL is read as version-bound data and is not executed on iPad. This application contains no x86 execution layer or JIT. See [RESOURCES](docs/RESOURCES.md) for the accepted layout and exact source hash.

Local default hit effects now use the original SD9 bank. Capable displays are requested at up to 120 Hz, with interpolated note rendering over the original 60 Hz simulation. See [sound recovery](docs/STAGE_SOUND.md) and [display timing](docs/RENDER_TIMING.md) for implemented behavior, remaining gaps and device checks.

The menu flow now includes original title, selection and result artwork, recovered difficulty switching and voices, session results and replay. Controls remain native and the original AFP page samples are static; full original menus are not yet recovered. Hold/Switch notes now share the original continuous SD9 loop. See [menu coverage](docs/MENU_PRESENTATION.md) and [loop lifecycle](docs/STAGE_CONTINUOUS_SOUND.md).
