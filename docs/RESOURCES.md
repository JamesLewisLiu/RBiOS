# Original resources

Select a directory retaining this original layout:

~~~text
GameRoot/
  modules/reflecbeat.dll
  data/
    graphics_characard/
    graphics_font/
    graphics_jacket2/
    graphics_rb6/
    graphics00/
    others/
    sound/
      ply2/
      2dx2/
      ...
~~~

Keep original names, extensions, formats and bytes. Select the common parent of data and modules using Original directory. A security-scoped bookmark retains access; import does not pre-extract, copy or transcode the game directory.

The catalog bridge verifies this exact build:

~~~text
MBR:J:A:A:2019121100
ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8
~~~

The DLL is data, not executable code on iPad. Other builds need independent analysis.

| Format | Implemented reading | Limits |
| --- | --- | --- |
| PLY / RBFF v20 | 44-byte header, objects, commands, extras, preparation and scheduling | No full original replay/device validation |
| IFS | KBinXML manifest, member ranges and bytes | No general super-IFS merging; current files do not require it |
| Textures | BGRA argb8888rev, AVSLZ, zero-compressed-length pixels, borders and UV crop | Not an arbitrary IFS image decoder |
| SD9 / 2dx | Original RIFF wrapper, stereo PCM16/MS ADPCM and fact count | Single-entry 2dx; unsupported layouts fail |
| Fonts | Original files retained | Game font layout not restored; shell uses system fonts |
| AFP | AFP/BSI/GE2D exported symbols, matrices, images, labels and supported scripts | Full VM, original pages and dynamic bindings remain incomplete |

Images and PCM are created only in memory. Audio is decoded in blocks for AVAudioPlayerNode. The stage uses its sample clock; timestamped touches share the monotonic host reference. The Apple archive and simulator tests pass; device synchronization remains unmeasured. AFP transforms original geometry corners for SpriteKit warps rather than exporting animation images. Stage-frame rectangular masks use the same coordinate transform. All 86 large character cards are complete single bitmaps; source cut edges are documented in [CHARACTER_RESOURCES](CHARACTER_RESOURCES.md).

rbinspect GAME_ROOT audits PLY/IFS/SD9/2dx without writing PNG, WAV or replacement packages. [VALIDATION](VALIDATION.md) records coverage.

python -B tools/hash_original.py creates a local snapshot and refuses to overwrite it. Add --verify to check the file set, sizes and SHA-256. The current 4,834-file snapshot was created during implementation; it is not a pre-session baseline.
