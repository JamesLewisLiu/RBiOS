"""Hash original resources without converting, copying, or changing any of them."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / '.build/original-resources.json'

def snapshot(game):
    paths = [game / 'modules/reflecbeat.dll', *sorted((game / 'data').rglob('*'))]
    records = []
    for path in paths:
        if not path.is_file():
            continue
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(chunk)
        records.append({'path': path.relative_to(game).as_posix(),
                        'bytes': path.stat().st_size, 'sha256': digest.hexdigest()})
    if not any(x['path'] == 'modules/reflecbeat.dll' for x in records) or len(records) < 2:
        raise ValueError('expected original modules/reflecbeat.dll and data/')
    return records

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--game-root', type=Path, default=ROOT.parent)
    args = parser.parse_args()
    records = snapshot(args.game_root.resolve())
    if args.verify:
        if json.loads(MANIFEST.read_text(encoding='utf-8')) != records:
            raise SystemExit('FAIL: original resource tree differs from recorded snapshot')
        print(f'PASS: {len(records)} original files match the snapshot')
    else:
        if MANIFEST.exists():
            raise SystemExit('snapshot already exists; use --verify to avoid replacing the baseline')
        MANIFEST.parent.mkdir(exist_ok=True)
        MANIFEST.write_text(json.dumps(records, indent=2) + '\n', encoding='utf-8')
        print(f'Recorded {len(records)} original files in {MANIFEST}')
