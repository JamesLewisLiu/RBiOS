# Original note style and draw descriptions

NoteGraphic restores head/background/style selection at 0x10155F10 and angle dispatch at 0x10155800. drawNoteGraphic exposes original parameters from 0x10155A00 for platform rendering.

## Inputs

NoteGraphicInput consumes prepared chart fields: color +280, Top +284, kind +288, side +328, special +16, destination +352, route flags +372, widthMode +400 and flags +300. presentationFlags is the original extra u8 argument. Skin selection remains separate; names resolve in the selected original package.

| Style | Original selection | Presentation |
| --- | --- | --- |
| 0 | obj_gold_red/blue | Original bitmap |
| 1 | obj_normal_red/blue | Original bitmap |
| 2 | long_head_red/blue and long_head_bg_red/blue | Original head/background; long component owns body/tail |
| 3 | obj_top_red/blue, or normal when presentationFlags&8 | Original bitmap |
| 4 | long_head_top_red/blue and long_head_bg_top, or ordinary hold with flag 8 | Original bitmaps |
| 5/6 | obj_bigbang_big/small_red/blue, pools 31/32 | Original AFP subset via GamePresentation |
| 7/8 | swich_on/off, pools 23/24 or 25/26 | Original labels/scripts and independent pool clocks |

The complete selector retains Top, kind, side, special, widthMode and flag branches. Names come from original strings 0x1036BED0..0x1036BF00. Movie descriptors are at 0x1036BFD8 with 28-byte stride; initialization is 0x10164A10. Default colors now connect through OriginalAfpMoviePool/GamePresentation. Full skin configuration selection is unfinished.

## Draw contract

Original 0x10155A00 supplies X/Y, center alignment(3,3), rotation, uniform scale*appearanceScale and alpha. 0x1002B250 confirms alignment 3 is half the visible image size, giving anchor(.5,.5). Pixel size comes from original bitmap UV dimensions. Display scaling does not enter the original logical coordinates.

Styles 0/1/5/6 use original float pi=3.1415927 on side 1 and zero on side 0; other styles use the supplied trajectory angle. Hold background layer is 61. Original blend argument 4102 becomes AFP8 through 1002B0F0->1002A2D0 and is retained in afpBlend.

Switch style 7 selects swich_on and style 8 selects swich_off. held selects the movie's internal on/off label, not its resource name. A movie description requires actual AFP sampling; it is not already a static sprite. needsSpecialOverlay exposes unresolved JR presentation.

## Background observations

0x10152210 selects package 98/99/100, corresponding to gmbg_00/01/02. Each contains original gmbg_00 and gmbg_01 movies. 0x10151EF0 centers them at(384,680). A stored 384x614 image or bg_cover384x364 is not the entire presented background; original internal matrices apply. All six movies, 1,440 frames total, have independent reference samples.

gmframe_00 contains frame_up798x174 and frame_down798x232. gmparts contains normal lines 798x12 and Top line images 74x12. These observations do not restore the complete frame page. [STAGE_GRAPHIC](STAGE_GRAPHIC.md) records the recovered line/Top placement.

## Validation and limits

- NoteGraphicTests: 12 draw/selection checks.
- tools/verify_graphic.py: 1,920 original selector cases over colors, Top, kinds, sides, special, width and flags, with empty AFP-pool fixtures.
- 108 original 0x10155800 angle bit-pattern comparisons.
- Report: .build/graphic-parity.json.

Body/tail work is described in [LONG_NOTE_GRAPHIC](LONG_NOTE_GRAPHIC.md); supported AFP and platform integration in [AFP_TIMELINE](AFP_TIMELINE.md) and [GAME_PRESENTATION](GAME_PRESENTATION.md). Full AFP VM, JR/collection effects, special overlays and skin configuration are unfinished. No end-to-end original draw or device pixel equivalence is claimed.
