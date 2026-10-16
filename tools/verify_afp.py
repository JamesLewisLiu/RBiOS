"""Compare raw AFP/BSI bindings and UV pixels with independent IFS tooling.

No game execution, network, extracted images, or resource rewrites.
"""
from io import BytesIO
from pathlib import Path
import hashlib
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.tools'))
from ifstools import IFS
from PIL import Image

exe = ROOT / '.build' / ('rbafp.exe' if sys.platform == 'win32' else 'rbafp')
report = {'mode': 'offline in-memory only', 'comparisons': []}
selections = {
    'gmobject': ['obj_normal_red', 'obj_normal_blue', 'obj_top_red', 'obj_gold_blue', 'long_head_red'],
    'gmframe_00': ['frame_down'],
    'title': ['start'],
}
for package, requested in selections.items():
    path = ROOT.parent / 'data' / 'graphics_rb6' / (package + '.ifs')
    archive = IFS(str(path), super_disable=True)
    try:
        files = {f.full_path.replace('\\', '/'): f for f in archive.tree.all_files}
        for name in requested:
            bitmap = files.get('tex/' + name + '.png')
            if bitmap is not None:
                expected = Image.open(BytesIO(bitmap.load(use_cache=False, crop_to_uvrect=True))).convert('RGBA').tobytes('raw', 'BGRA')
                actual = subprocess.check_output([str(exe), '--bitmap', str(path), name])
                if actual != expected:
                    raise AssertionError('bitmap differs: ' + package + ':' + name)
                report['comparisons'].append({'package': package, 'kind': 'bitmap', 'name': name,
                    'bytes': len(actual), 'sha256': hashlib.sha256(actual).hexdigest()})
        # Compare every movie and BSI from these packages, not only chosen images.
        for fullname, member in files.items():
            if not fullname.startswith('afp/') or fullname.endswith('.xml'):
                continue
            op = '--bsi' if fullname.startswith('afp/bsi/') else '--movie'
            name = fullname.rsplit('/', 1)[-1]
            expected = member.load(convert_kbin=False)
            actual = subprocess.check_output([str(exe), op, str(path), name])
            if actual != expected:
                raise AssertionError('movie/BSI differs: ' + package + ':' + fullname)
            report['comparisons'].append({'package': package, 'kind': op[2:], 'name': name,
                'bytes': len(actual), 'sha256': hashlib.sha256(actual).hexdigest()})
    finally:
        archive.close()
(ROOT / '.build' / 'afp-parity.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print('PASS', len(report['comparisons']), 'independent AFP bitmap/movie/BSI comparisons')
