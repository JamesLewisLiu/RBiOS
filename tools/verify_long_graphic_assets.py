"""Read original long-note assets in memory; never export converted resources."""
from io import BytesIO
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import types

ROOT = Path(__file__).resolve().parents[1]
reference = ROOT / '.tools/afp-reference'
for name, relative in [('bemani', 'bemani'), ('bemani.format', 'bemani/format'),
                       ('bemani.format.afp', 'bemani/format/afp')]:
    module = types.ModuleType(name)
    module.__path__ = [str(reference / relative)]
    sys.modules[name] = module
sys.path.insert(0, str(ROOT / '.tools'))
from bemani.format.afp.swf import SWF
from ifstools import IFS
from PIL import Image

exe = ROOT / '.build' / ('rbafp.exe' if sys.platform == 'win32' else 'rbafp')
sampler = ROOT / '.build' / ('rbafptimeline.exe' if sys.platform == 'win32' else 'rbafptimeline')
report = {'mode': 'original resource bytes, in-memory decoding only', 'bitmaps': [], 'movies': []}
for package in ['gmobject', 'gmexp_effect']:
    path = ROOT.parent / 'data/graphics_rb6' / (package + '.ifs')
    archive = IFS(str(path), super_disable=True)
    try:
        files = {f.full_path.replace('\\', '/'): f for f in archive.tree.all_files}
        textures = ([f'{part}_{color}' for part in ['long_body', 'long_tail', 'long_tail_bg']
                     for color in ['red', 'blue', 'top']] + ['long_tail_switch']) if package == 'gmobject' else []
        for name in textures:
            member = files['tex/' + name + '.png']
            image = Image.open(BytesIO(member.load(use_cache=False, crop_to_uvrect=True))).convert('RGBA')
            expected = image.tobytes('raw', 'BGRA')
            actual = subprocess.check_output([str(exe), '--bitmap', str(path), name])
            assert actual == expected, name
            report['bitmaps'].append(dict(package=package, resource=name, size=image.size,
                                           bgra_sha256=hashlib.sha256(actual).hexdigest()))
        movies = ['swich_long_body_on', 'swich_long_body_off', 'long_tail_bg_switch',
                  'eff_long_tail_red', 'eff_long_tail_blue', 'eff_justcollection'] if package == 'gmobject' else ['exp_loop']
        for name in movies:
            movie = SWF(name, files['afp/' + name].load(convert_kbin=False),
                        files['afp/bsi/' + name].load(convert_kbin=False))
            movie.parse()
            definition = next(t for t in movie.tags if t.id == movie.exported_tags[name])
            rows = ''.join(f'{name} {f}\n' for f in range(len(definition.frames)))
            result = subprocess.run([str(sampler), '--sample', str(path)], input=rows,
                                    text=True, capture_output=True)
            item = dict(package=package, resource=name, frames=len(definition.frames),
                        labels=definition.labels, sampled=len(result.stdout.splitlines()),
                        success=result.returncode == 0)
            if result.returncode: item['error'] = result.stderr.strip()
            report['movies'].append(item)
    finally:
        archive.close()
(ROOT / '.build/long-graphic-assets.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report))
if any(not movie['success'] for movie in report['movies']):
    sys.exit(1)
