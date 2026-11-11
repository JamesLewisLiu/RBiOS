# Documentation index

## Project and delivery

| Document | Purpose |
| --- | --- |
| [STATUS](STATUS.md) | Implemented code, unfinished behavior and verification limits |
| [BUILD](BUILD.md) | macos-15 Actions, Xcode, unsigned packaging and minimum-OS checks |
| [VALIDATION](VALIDATION.md) | Host checks, original-instruction comparisons and resource audits |
| [RESOURCES](RESOURCES.md) | Original directory layout and runtime format support |
| [MIGRATION](MIGRATION.md) | Windows startup, original addresses and restored interfaces |
| [Machine-readable results](validation/local-results.json) | Saved validation summary |
| [Historical plans](plans/01-native-reimplementation.md) | Preserved decisions; follow numbered revisions |
| [Delivery revision](plans/06-english-local-delivery.md) | English cleanup, local commit and user-managed push |

## Core execution

| Document | Area |
| --- | --- |
| [ROUTE](ROUTE.md) | Route generation and redirection |
| [MOTION](MOTION.md) | Velocity timelines, geometry and integration |
| [NOTE_FRAME](NOTE_FRAME.md) | Note phases and per-tick transitions |
| [TOUCH](TOUCH.md) | Judgment geometry and touch queries |
| [TOUCH_INPUT](TOUCH_INPUT.md) | Original 15-slot association |
| [CPU](CPU.md) | Random distributions and CPU judgment |
| [SCORE](SCORE.md) | Scoring, combo, rates and result calculations |
| [STAGE_SCORE](STAGE_SCORE.md) | Stage score initialization |
| [GAMEPLAY](GAMEPLAY.md) | Judgment and submission call chain |
| [STAGE_SESSION](STAGE_SESSION.md) | Full-chart scheduling, ordering and lifecycle |
| [PROTOCOL](PROTOCOL.md) | Recovered fields, encoding and network boundaries |

## Original graphics

| Document | Area |
| --- | --- |
| [AFP](AFP.md) | Archive names, bitmap UVs, movies, BSI and geometry |
| [AFP_TIMELINE](AFP_TIMELINE.md) | Supported display lists, scripts, labels and clocks |
| [AFP_RELEASE_ORDER](AFP_RELEASE_ORDER.md) | Original cleanup and LIFO reuse |
| [NOTE_GRAPHIC](NOTE_GRAPHIC.md) | Head styles and draw parameters |
| [LONG_NOTE_GRAPHIC](LONG_NOTE_GRAPHIC.md) | Hold and Switch geometry and assets |
| [STAGE_LONG_PRESENTATION](STAGE_LONG_PRESENTATION.md) | Per-tick integration and catalog audits |
| [STAGE_LABEL_GRAPHIC](STAGE_LABEL_GRAPHIC.md) | Original normal-layout score labels and AFP bindings |
| [STAGE_SCORE_GRAPHIC](STAGE_SCORE_GRAPHIC.md) | Original score/combo glyphs and instruction comparison |
| [SCORE_DISPLAY](SCORE_DISPLAY.md) | Original per-tick score counter update |
| [STAGE_GRAPHIC](STAGE_GRAPHIC.md) | Judgment lines, Top chips and backgrounds |
| [STAGE_FRAME](STAGE_FRAME.md) | Original panels, named texture bindings, masks and child clocks |
| [CHARACTER_RESOURCES](CHARACTER_RESOURCES.md) | Complete portrait bitmaps and source artwork boundaries |
| [GAME_PRESENTATION](GAME_PRESENTATION.md) | Core-to-Apple presentation boundary |
| [STAGE_SOUND](STAGE_SOUND.md) | Original local hit sounds and pending audio recovery |
| [STAGE_CONTINUOUS_SOUND](STAGE_CONTINUOUS_SOUND.md) | Original Hold/Switch loop lifecycle |
| [MENU_SELECTION](MENU_SELECTION.md) | Recovered difficulty selection and original instruction comparisons |
| [MENU_PRESENTATION](MENU_PRESENTATION.md) | Original static title/selection/result artwork and remaining page bindings |
| [RENDER_TIMING](RENDER_TIMING.md) | Up to 120 Hz presentation over 60 Hz logic |

Authored documentation, interface strings and comments are English. Original game metadata and assets retain their original encoding and contents.

## Original execution recovery

- [FIDELITY_AUDIT](FIDELITY_AUDIT.md): recovered components versus rejected replacement flow.
- [ORIGINAL_BOOT_CHAIN](ORIGINAL_BOOT_CHAIN.md): loader, AVS, EA3 and original game entry.
- [ORIGINAL_TEST_MENU](ORIGINAL_TEST_MENU.md): original tables and decisions; subscenes remain unported.
- [AVS_BOOT_CONFIG](AVS_BOOT_CONFIG.md): bounded original AVS configuration transformation.

- [AVS_CONFIG_IMPORT](AVS_CONFIG_IMPORT.md): original 48-field configuration map, defaults and supported typed import.
- [Filesystem boot](avs-filesystem-boot.md): original registration/mount ordering and explicit backend boundary.
- [ORIGINAL_TEST_READINESS](ORIGINAL_TEST_READINESS.md): original TEST wait-state decisions and EA3 call boundary.

- [Accepted complete port plan](NATIVE_PORT_PLAN.md): execution order and updated storage/network decisions.
- [AVS descriptors](AVS_DESCRIPTORS.md): original token allocation and cleanup groups.
- [AVS property paths](AVS_PROPERTY_PATH.md): original query grammar and node lookup.
- [AVS file paths](avs-path-resolution.md): original normalization and prefix hashes.
- [Default filesystem reads](avs-default-file.md): actual host reads and remaining mount ownership.
- [Cabinet configuration writes](CABINET_CONFIG_STORE.md): backup and replacement platform boundary.

- [AVS XML metadata](AVS_XML_METADATA.md): original type aliases and count/size conversion.

- [Original startup entry](ORIGINAL_STARTUP_ENTRY.md): default entry, executed prefix and explicit stop.
- [AVS XML properties](AVS_XML_PROPERTY.md): original typed XML event semantics.
- [AVS mutexes](AVS_MUTEXES.md): real native resources and descriptor lifetime.
- [AVS thread context](AVS_THREAD_CONTEXT.md): registered calling-thread TLS and remaining worker initialization.
- [AVS mount registry](avs-mount-registry.md): read-only fs mounting and original option parser.

- [AVS thread bootstrap policy](AVS_THREAD_BOOTSTRAP.md): original stack, count, arena and affinity calculations.
- [AVS native workers](AVS_NATIVE_WORKERS.md): real worker lifecycle and remaining admission/recycling dependencies.

- [AVS resource pools](AVS_RESOURCE_POOL.md): real finite allocations, descriptor ownership and usage queries.
- [AVS tasks](AVS_TASKS.md): original task admission, worker recycling and failure side effects.
- [AVS alarms](AVS_ALARMS.md): original schedules, callback order and reserved worker boundary.
