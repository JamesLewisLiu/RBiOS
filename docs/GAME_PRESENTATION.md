# Original resources to platform draws

GamePresentation consumes restored draw descriptions and supplies original package binding, movie clocks and transformed geometry. It is a platform adapter, not replacement judgment logic. It writes no asset cache or scores.

Create GamePresentation before StageSession. onNoteCreated/onNoteReleased acquire and return complete hidden bindings; onPresentationFrame advances each original 60 Hz tick. Display frames read the prepared snapshot at the same tick. A batched advance still processes all intermediate lifetimes and controls.

Static draws retain original visible dimensions, anchor, angle, nonuniform scale, alpha and blend. AFP applies display-list matrices first, then movie-location anchor and external scale/rotation. Swift uses SKWarpGeometryGrid over those four corners, converting original Y-down to SpriteKit Y-up. z=-originalLayer preserves layer direction; command order resolves ties. Texture keys include the original package to avoid cross-package name collisions.

The initial background is original gmbg_00. Local finalRate >=0.69999999 switches to gmbg_01 at original frame 1. Judgment lines/Top assets come from gmparts. Hold bodies/tails use original bitmaps; Switch bodies/caps, tail effects and Bigbang use original AFP. Files are not exported into substitute resources.

The stage frame uses the original `gmframe_00/gmframe` movie at (-15,0), layer 22, with a top-left anchor. Its normal root stops at frame 3 while bottom-panel children continue from the stage clock. Original same-package textures supply round, difficulty, player labels and orbs. Inherited rectangular clip quadrilaterals pass through the same outer transform as sprite corners and become nested SpriteKit crop nodes. Unbound account text and portrait dummy shapes are hidden and reported in `PresentationFrame.unresolved`. See [STAGE_FRAME](STAGE_FRAME.md).

OriginalAfpMoviePool owns all 37 descriptor types and independent persistent clocks. prepareSwitchMovieDraw preserves hidden playback. Cleanup follows [AFP_RELEASE_ORDER](AFP_RELEASE_ORDER.md): long components, complete Switch body/cap/collection arrays and tail, then head on/off. Same-tick cross-note ordering and complete exhaustion semantics need separate validation. Missing JR/collection queue consumers remain in PresentationFrame.unresolved.

RBGameSession uses this same host-audited pipeline and bridges corners, in-memory images and transient scores to Swift. GameViewController drives it with the audio sample clock. UIKit menus/results are platform entry, not recovered original page scenes.

## Validation

~~~powershell
python -B tools/build_host.py
python -B tools/audit_presentation.py ..
.build/rbgamepresentation.exe .. 1 6k60_lastboss_w 1
~~~

Before the frame integration, all 2,309 enabled catalog charts passed with 522,082 sampled display frames, 22,085,142 draw commands and 131 original bitmaps. Movie controls advance every tick; full draw sampling uses stride 30. After frame integration, four charts across all difficulties pass with 738 samples, 36,542 draws and 139 bitmaps. The targeted WHITE HARD chart passes all 7,498 ticks, 635,983 draws and 195 bitmaps, including Switch/Bigbang/hold objects. The full catalog drawing audit has not been repeated with the new frame.

Reports remain in ignored .build output inside the analysis directory. They contain metadata, not exported media. Host command/bitmap checks do not establish SpriteKit pixels, device performance or audible synchronization. The prior Apple archive and 21 simulator tests passed, and a user screenshot shows stage completion after the viewport correction. The new frame and nested-mask GPU checks require the next Actions run and device installation.
