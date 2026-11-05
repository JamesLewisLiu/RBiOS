# Original character artwork boundaries

The Settings preview displays the complete original character bitmap. Some illustrations already reach the source image boundaries, producing hard cuts in the artwork. The missing stage frame, text and dynamic interface objects are a separate migration task; changing the preview scale cannot restore pixels absent from a source image.

## Original package inventory

All 86 supplied packages, data/graphics_characard/characard_b_000.ifs through characard_b_085.ifs, contain exactly one visible 768 x 1360 bitmap each. They contain texture metadata and pixels, with no AFP movie, GE2D geometry or second illustration component. The runtime firstTextureAt reader selects the sole bitmap in these packages.

For characard_b_000, the original tex/texturelist.xml declares:

| Field | Stored values | Pixel interpretation |
| --- | --- | --- |
| Sheet size | 1024 2048 | Original texture sheet dimensions |
| imgrect | 0 1540 0 2724 | Coordinates divided by two; padded image 770 x 1362 |
| uvrect | 2 1538 2 2722 | Coordinates divided by two; visible image 768 x 1360 |

The visible BGRA pixels have SHA-256 a62ace9543b663e8393ceaecd1497cc4fc57578f524ff6900d2c24862750e8e1. Their nonzero alpha bounding box is [0, 74, 768, 1312], with right/bottom coordinates exclusive.

The following measurements count pixels whose alpha is greater than 16, using top-left image coordinates:

| Boundary | First position | Last position | Covered pixels |
| --- | --- | --- | --- |
| Left, X=0 | Y=836 | Y=1104 | 269 |
| Right, X=767 | Y=181 | Y=530 | 325 |
| Top, Y=0 | None | None | 0 |
| Bottom, Y=1359 | None | None | 0 |

The left leg and right hair/cape therefore meet the original image boundaries. Keeping all original pixels still shows these cuts. No inferred stitching, expansion or replacement artwork is used.

Related original packages do not provide an extension to this bitmap. graphics_rb6/characard_r_00.ifs contains 86 images of 700 x 276 pixels. characard_s_00.ifs contains 88 images of 160 x 230 pixels. The characard_select packages contain selection interface art and movie definitions.

## Original code evidence

The saved modules - Copy/reflecbeat.dll.c pseudocode identifies 0x10079B00 resolving characard_b_%03d through afpu_get_image_info, and 0x100B2820 loading the matching graphics_characard IFS into resource group 7. Initialization at 0x100B9B70 binds the same bitmap to named characard_b and characard_b_sdw scene objects. These references describe original resource selection; they do not establish missing bitmap pieces or newly observed native execution. This pass used saved decompilation and original package bytes, without executing the game.

## Reproduce the resource check

Install the optional analysis dependencies used by tools/verify_afp.py, including ifstools and Pillow, in the project's .tools directory or provide --dependency-dir. Supply the original directory containing data. Keep reports outside original resource directories.

~~~powershell
python -B tools/verify_character_resources.py E:/MBR-2019121100/IDA --dependency-dir E:/MBR-2019121100/IDA/RBiOS/.tools --report E:/MBR-2019121100/IDA/RBiOS/.build/character-resource-evidence.json
~~~

The verifier reads all 86 original packages, checks manifest dimensions, decodes one bitmap at a time in memory, records source and pixel hashes, and verifies the first portrait's edge measurements. It writes only a JSON evidence report and refuses to overwrite an existing report. It does not write PNG, WAV, extracted archives or converted game resources. These checks establish source-image completeness, not full original scene equivalence or iPad GPU output.
