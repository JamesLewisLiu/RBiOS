# Original stage score and combo graphics

The ordinary battle-score branch restores the original digit selection and placement from `reflecbeat.dll` entry `0x10152600`. Images are read directly from `data/graphics_rb6/gmparts.ifs` and decoded only in memory. No font replacement, generated digit atlas or converted image bundle is used.

## Original evidence

The version-bound DLL SHA-256 is `ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8`. Saved pseudocode in `modules - Copy/reflecbeat.dll.c` and original PE pointer tables identify the following source families. Texture dimensions were independently read from the original IFS texture manifest; stored UV coordinates are divided by two.

| Original pointer table | Original texture family | Visible dimensions |
| --- | --- | --- |
| `0x1036BD70`, `0x1036BD98` | `score_as_small_red_00..09`, `score_as_small_blue_00..09` | 44 x 56 |
| `0x1036BE10`, `0x1036BE60` | `combo_big_as_yellow_00..09`, `combo_big_as_white_00..09` | 84 x 106 |
| `0x1036BE38`, `0x1036BE88` | `combo_small_as_yellow_00..09`, `combo_small_as_white_00..09` | 28 x 36 |
| Literal strings in `0x10152600` | `combo_big_yellow/white` | 96 x 20 |
| Literal strings in `0x10152600` | `combo_small_yellow/white` | 70 x 14 |

The original draw routine queries named anchors from `gmbase_00`, using the names in tables `0x1036BEB0`, `0x1036BEC0` and `0x1036BEC8`:

| Element | Local player | Rival |
| --- | --- | --- |
| Score digits | `score_as_you`: (384,624) | `score_as_rival`: (384,516) |
| Combo digits | `combo_as_you`: (384,737) | `combo_as_rival`: (384,300) |
| Combo word | `combo_you`: (384,807) | `combo_rival`: (384,269) |

Both sides use layer 75. Scores draw least-significant digits first, at pitch 44, with original half-width/half-height anchors (22,28). Starting X moves by 22 for two digits, 44 for three, and 66 for four or more. The loop emits at most four digits. Zero still emits one digit. Values above 9999 retain only the four least-significant digits. The loop counter starts at zero, increments after a draw, and exits when it reaches four.

Combo pitches are 84 for the player and 28 for the rival. The initial X offset is half a pitch for two digits, one pitch for three, and one-and-a-half pitches for four or more. Digits use their original centers, and the word uses centered alignment. The local yellow/white branch reads whether the player's miss field is zero; the rival branch reads the original full-combo flag at player offset 104. Color and yellow-state inputs remain separate from the numeric layout.

`0x10071440` constructs `CULTexture`; it is not the stage number-rendering routine. The separate `CULNumberTexture` family initializes formatted digit sprites at `0x10071870`, builds digit names at `0x10071A80`, computes alignment at `0x10071AB0`, and updates digits at `0x10071B00`. The restored ordinary stage branch directly draws its glyphs through `0x1002AC30` and does not substitute this generic helper for `0x10152600`.

## Independent instruction comparison

`tools/verify_stage_score_graphic.py` executes the original `0x10152600` machine instructions in the existing isolated Unicorn fixture. The original game process and networking are never started. AFP named-position lookup and renderer virtual methods are capture-only fixtures. The actual original arithmetic, branch decisions, digit pointer-table selection and loop limits execute unchanged.

The expanded digit-boundary audit passed **120 cases and 1,704 draws**. It covers values 0,1,9,10,99,100,999,1000,9999,10000,99999,100000,12345,987654321,2147483647; both player colors; and both yellow/white states for each side. Opponent score and combo values differ from the local values to detect accidental shared-state usage. Each captured texture name, position, anchor and layer is compared with the native `--batch` output. The verifier also checks the original capture itself for exactly one through four digits per counter, so an accidental extra digit or truncated capture cannot be hidden by matching native output. This corrects an earlier documentation-only claim of five digits; the existing four-digit implementation was already correct.

```powershell
python -B G:/vibe/RBiOS/tools/verify_stage_score_graphic.py --analysis-root E:/MBR-2019121100/IDA/RBiOS --executable E:/MBR-2019121100/IDA/RBiOS/.build/active/rbstagescoregraphic.exe --report E:/MBR-2019121100/IDA/RBiOS/.build/stage-score-graphic-parity.json
```

This comparison establishes ordinary-mode draw-command parity with explicit AFP position fixtures. It does not execute the full AFP VM, validate every scene transition, or compare original Windows/iPad pixels. Course/life mode, animated score transitions and the full original scene remain separate work.

## Fresh decompiler corroboration

On 2026-11-10, Official Hex-Rays IDA MCP Server 20261028.0.3 opened an analysis copy of the existing database through idalib. The old IDB was migrated to I64 inside the analysis output directory. Its `execute_python` tool freshly decompiled 0x10152600, 0x10155460 and 0x10164FC0 using `db.pseudocode.get_text`. The results corroborate the digit layout and displayed-score input. The database lease was closed after inspection. Evidence remains in the ignored `official-ida-score-session.log`; no decompilation output or original resources are shipped.

The active client configuration points to a removed older plugin directory; this pass invoked the installed official server over stdio without changing client configuration. Future decompilation should use the Official IDA MCP Server.

After integration, four charts across all difficulties pass with 738 sampled frames, 45,289 draws and 192 original bitmaps. Original layout animation, score labels and original-engine/device pixel comparisons remain unfinished.
