"""Exercise the native stage/presenter on original files without writing decoded assets."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('game_root', type=Path)
parser.add_argument('--limit', type=int, default=10000)
parser.add_argument('--filter', default='')
parser.add_argument('--stride', type=int, default=30)
args = parser.parse_args()
out = PROJECT / '.build'
binary = out / ('rbgamepresentation.exe' if sys.platform == 'win32' else 'rbgamepresentation')
with (out / 'presentation-audit.log').open('w', encoding='utf-8') as log:
    result = subprocess.run([str(binary), str(args.game_root.resolve()), str(args.limit),
                             args.filter, str(args.stride)], cwd=PROJECT, stdout=log,
                            stderr=subprocess.STDOUT, text=True)
text = (out / 'presentation-audit.log').read_text(encoding='utf-8')
if result.returncode:
    print(text[-4000:])
    raise SystemExit(result.returncode)
summary = re.search(r'Presentation: (\d+) songs, (\d+) sampled frames, (\d+) draws, (\d+) original bitmaps', text)
if not summary:
    raise SystemExit('presentation summary missing')
record = dict(zip(('charts', 'sampled_frames', 'draws', 'original_bitmaps'), map(int, summary.groups())))
record.update(stride=args.stride, filter=args.filter,
              object_kinds=list(map(int, re.search(r'Object kinds:([\d ]+)', text)[1].split())),
              unresolved=re.findall(r'^UNRESOLVED (.+)$', text, flags=re.M),
              mode='local auto + CPU0, original files decoded only in memory',
              limits=['host draw-command/bitmap integration, not Apple pixels or device play',
                      'draw commands sampled at stride; original playback updated every 60 Hz tick',
                      'no external connection, profile storage or score upload'])
(out / 'presentation-audit.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
print(json.dumps(record, ensure_ascii=False, indent=2))
