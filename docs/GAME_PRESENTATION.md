# Original resources to platform draws

GamePresentation consumes restored draw descriptions and supplies original package binding, movie clocks and transformed geometry. It is a platform adapter, not replacement judgment logic. It writes no asset cache or scores.

Create GamePresentation before StageSession. onNoteCreated/onNoteReleased acquire and return complete hidden bindings; onPresentationFrame advances each original 60 Hz tick. Display frames read the prepared snapshot at the same tick. A batched advance still processes all intermediate lifetimes and controls.

Static draws retain original visible dimensions, anchor, angle, nonuniform scale, alpha and blend. AFP applies display-list matrices first, then movie-location anchor and external scale/rotation. Swift uses SKWarpGeometryGrid over those four corners, converting original Y-down to SpriteKit Y-up. z=-originalLayer preserves layer direction; command order resolves ties. Texture keys include the original package to avoid cross-package name collisions.

The initial background is original gmbg_00. Local finalRate >=0.69999999 switches to gmbg_01 at original frame 1. Judgment lines/Top assets come from gmparts. Hold bodies/tails use original bitmaps; Switch bodies/caps, tail effects and Bigbang use original AFP. Files are not exported into substitute resources.

OriginalAfpMoviePool owns all 37 descriptor types and independent persistent clocks. prepareSwitchMovieDraw preserves hidden playback. Cleanup follows [AFP_RELEASE_ORDER](AFP_RELEASE_ORDER.md): long components, complete Switch body/cap/collection arrays and tail, then head on/off. Same-tick cross-note ordering and complete exhaustion semantics need separate validation. Missing JR/collection queue consumers remain in PresentationFrame.unresolved.

RBGameSession uses this same host-audited pipeline and bridges corners, in-memory images and transient scores to Swift. GameViewController drives it with the audio sample clock. UIKit menus/results are platform entry, not recovered original page scenes.

## Validation

~~~powershell
python -B tools/build_host.py
python -B tools/audit_presentation.py ..
.build/rbgamepresentation.exe .. 1 6k60_lastboss_w 1
~~~

All 2,309 enabled catalog charts pass with 522,082 sampled display frames, 22,085,142 draw commands and 131 original bitmaps. Movie controls advance every tick; full draw sampling uses stride 30. The targeted WHITE HARD chart passes all 7,498 ticks, 561,003 draws and 130 bitmaps, including Switch/Bigbang/hold objects.

Reports remain in .build/presentation-audit.json and presentation-every-tick.log. They contain metadata, not exported media. Host command/bitmap checks do not establish SpriteKit pixels, device performance or audible synchronization. Apple compilation/device play remain pending.
