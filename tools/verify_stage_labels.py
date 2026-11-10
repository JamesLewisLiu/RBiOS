"""Compare native normal score labels with original AFP placements and texture metadata."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import types


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--dependencies', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    for folder in ['data', 'modules']:
        if args.report.resolve().is_relative_to((args.game_root / folder).resolve()):
            raise ValueError('Report must be outside original resources')
    if args.report.exists():
        raise ValueError('Report already exists')
    base = args.dependencies / 'afp-reference'
    for name, path in [('bemani', base / 'bemani'), ('bemani.format', base / 'bemani/format'),
                       ('bemani.format.afp', base / 'bemani/format/afp')]:
        module = types.ModuleType(name)
        module.__path__ = [str(path.resolve())]
        sys.modules[name] = module
    sys.path.insert(0, str(args.dependencies.resolve()))
    from ifstools import IFS
    from bemani.format.afp.swf import SWF
    from kbinxml import KBinXML
    package_path = args.game_root / 'data/graphics_rb6/gmparts.ifs'
    archive = IFS(str(package_path), super_disable=True)
    try:
        files = {p.full_path.replace('\\', '/'): p for p in archive.tree.all_files}
        movie = SWF('gmbase_00', files['afp/gmbase_00'].load(convert_kbin=False),
                    files['afp/bsi/gmbase_00'].load(convert_kbin=False))
        movie.parse()
        placements = {}

        def walk(value):
            if isinstance(value, dict):
                if value.get('movie_name') in ('score_you', 'score_rival'):
                    placements.setdefault(value['movie_name'], value)
                for child in value.values():
                    walk(child)
            elif isinstance(value, list):
                for child in value:
                    walk(child)

        walk(movie.as_dict())
        xml = KBinXML(files['tex/texturelist.xml'].load(convert_kbin=False)).xml_doc
        sizes = {}
        for sheet in xml:
            for image in sheet.findall('image'):
                uv = list(map(int, image.find('uvrect').text.split()))
                sizes[image.attrib['name']] = ((uv[1] - uv[0]) / 2, (uv[3] - uv[2]) / 2)
        lines = subprocess.check_output([str(args.executable), '--describe'], text=True).splitlines()
        assert len(lines) == 4
        for line in lines:
            color, name, x, y, layer, ax, ay = line.split()
            side = 1 if '_rival_' in name else 0
            target = 'score_rival' if side else 'score_you'
            expected_name = target + ('_blue' if int(color) ^ side else '_red')
            assert name == expected_name
            placement = placements[target]
            matrix, origin = placement['transform'], placement['rotation_origin']
            assert [matrix[k] for k in ['a', 'b', 'c', 'd']] == [1, 0, 0, 1]
            assert (float(x), float(y)) == (matrix['tx'], matrix['ty'])
            width, height = sizes[name]
            assert (width, height) == (124, 14)
            assert (float(ax), float(ay)) == (origin['x'] / width, origin['y'] / height)
            assert int(layer) == 76  # Original 10153E80 initialization, independent code evidence.
        report = {'status': 'passed', 'labels': 4, 'colors': 2,
                  'package_sha256': hashlib.sha256(package_path.read_bytes()).hexdigest(),
                  'reference': 'Original AFP normal placement and texture metadata',
                  'limits': 'Layer from native initialization; no complete AFP VM or device pixel comparison'}
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('PASS 4 original score labels across both colors')
    finally:
        archive.close()


if __name__ == '__main__':
    main()
