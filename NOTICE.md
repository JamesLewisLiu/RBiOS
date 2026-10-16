# References and attribution

Restored game logic comes from the local MBR:J:A:A:2019121100 binaries and existing decompilation supplied for this work. Original DLLs, decompilation output and resources are not included. This notice does not assign them a new license.

Format analysis references [mon/kbinxml](https://github.com/mon/kbinxml) 2.2 (MIT) and [mon/ifstools](https://github.com/mon/ifstools) 1.15. Host-only analysis loads them from .tools; they are not bundled with the app. Pixel comparisons use Pillow, audio uses FFmpeg, isolated instructions use Unicorn and disassembly uses Capstone. Package licenses remain with local installations.

AP2/BSI/GE2D interpretation references [DragonMinded/bemaniutils](https://github.com/DragonMinded/bemaniutils), fixed commit 1e66fcd780afb3088ce8e78c88615a143db492ee, under the Unlicense. The independent Python sampler is host-only. Its license is preserved in [docs/licenses/bemaniutils-UNLICENSE.txt](docs/licenses/bemaniutils-UNLICENSE.txt); coverage is in [AFP_TIMELINE](docs/AFP_TIMELINE.md).

KBinXML reference license:

```text
MIT License

Copyright (c) 2018 mon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
