"""Verify original character artwork and source-edge clipping, in memory only.

Reads original IFS files. Writes only JSON metadata; never writes decoded images.
Optional dependencies: ifstools and Pillow, as used by verify_afp.py.
"""
import argparse
from collections import Counter
import hashlib
from io import BytesIO
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_root', type=Path, help='Original directory containing data/')
    parser.add_argument('--dependency-dir', type=Path, default=ROOT / '.tools')
    parser.add_argument('--report', type=Path, default=ROOT / '.build/character-resource-evidence.json')
    args = parser.parse_args()
    sys.path.insert(0, str(args.dependency_dir.resolve()))
    from ifstools import IFS
    from PIL import Image

    directory = args.game_root / 'data/graphics_characard'
    paths = sorted(directory.glob('characard_b_*.ifs'))
    expected_names = [f'characard_b_{i:03d}.ifs' for i in range(86)]
    if [p.name for p in paths] != expected_names:
        raise ValueError('Character package family differs from the analyzed 2019121100 build')
    records = []
    for path in paths:
        archive = IFS(str(path), super_disable=True)
        try:
            files = {f.full_path.replace('\\', '/'): f for f in archive.tree.all_files}
            manifest = ET.fromstring(files['tex/texturelist.xml'].load(convert_kbin=True))
            images = []
            for sheet in manifest:
                for node in sheet.findall('image'):
                    name = node.attrib['name']
                    raw_uv = [int(v) for v in node.find('uvrect').text.split()]
                    raw_image = [int(v) for v in node.find('imgrect').text.split()]
                    if any(v % 2 for v in raw_uv + raw_image):
                        raise ValueError('Odd texture coordinate: ' + path.name)
                    uv = [v // 2 for v in raw_uv]
                    rect = [v // 2 for v in raw_image]
                    with Image.open(BytesIO(files['tex/' + name + '.png'].load(
                            use_cache=False, crop_to_uvrect=True))) as decoded:
                        image = decoded.convert('RGBA')
                    width, height = image.size
                    if (width, height) != (768, 1360) or (width, height) != (uv[1] - uv[0], uv[3] - uv[2]):
                        raise ValueError('Unexpected visible character dimensions: ' + path.name)
                    alpha = image.getchannel('A')
                    edges = {}
                    for edge, x, y in [('left', 0, None), ('right', width - 1, None),
                                       ('top', None, 0), ('bottom', None, height - 1)]:
                        positions = [i for i in range(height if y is None else width)
                                     if alpha.getpixel((x, i) if y is None else (i, y)) > 16]
                        edges[edge] = {'count': len(positions),
                                       'first': min(positions) if positions else None,
                                       'last': max(positions) if positions else None}
                    images.append({'name': name, 'size': [width, height],
                                   'sheet_size': [int(v) for v in sheet.find('size').text.split()],
                                   'raw_uvrect': raw_uv, 'raw_imgrect': raw_image,
                                   'uvrect': uv, 'imgrect': rect, 'alpha_bbox': alpha.getbbox(),
                                   'edge_alpha_above_16': edges,
                                   'uv_bgra_sha256': hashlib.sha256(image.tobytes('raw', 'BGRA')).hexdigest()})
            other = [name for name in files if not name.startswith(('tex/', '_canvas'))
                     and name not in ('magic', 'cversion')]
            if len(images) != 1 or other:
                raise ValueError('Unexpected character composition members: ' + path.name)
            records.append({'file': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                            'images': images, 'non_texture_members': other})
        finally:
            archive.close()
    first = records[0]['images'][0]
    if first['uv_bgra_sha256'] != 'a62ace9543b663e8393ceaecd1497cc4fc57578f524ff6900d2c24862750e8e1':
        raise ValueError('First character bitmap differs from the recorded original')
    expected_edges = {'left': (269, 836, 1104), 'right': (325, 181, 530),
                      'top': (0, None, None), 'bottom': (0, None, None)}
    for edge, expected in expected_edges.items():
        actual = first['edge_alpha_above_16'][edge]
        if (actual['count'], actual['first'], actual['last']) != expected:
            raise ValueError('First character alpha-edge evidence differs: ' + edge)
    report = {'mode': 'offline, in-memory decoding; metadata-only output',
              'packages': len(records), 'records': records}
    report_path = args.report.resolve()
    resource_path = args.game_root.resolve()
    for child in ('data', 'modules'):
        if report_path.is_relative_to(resource_path / child):
            raise ValueError('Report must not be written inside original resources')
    if report_path.exists():
        raise FileExistsError('Refusing to overwrite existing evidence report: ' + str(report_path))
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS: 86 original packages; one 768x1360 bitmap each; original edge clipping confirmed')
    print('Image count distribution:', dict(Counter(len(r['images']) for r in records)))
    print('Metadata report:', report_path)


if __name__ == '__main__':
    main()
