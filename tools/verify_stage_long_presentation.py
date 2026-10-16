"""Whole-catalog, every-tick presentation audit; no resource conversion/network."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
exe = ROOT / '.build' / ('rbstagelongpresentation.exe' if sys.platform == 'win32' else 'rbstagelongpresentation')

def run(configuration):
    side, mode = configuration
    suffix = f'side{side}-mode{mode}'
    output = ROOT / f'.build/long-presentation-{suffix}.json'
    error = ROOT / f'.build/long-presentation-{suffix}.log'
    with output.open('w', encoding='utf-8') as out, error.open('w', encoding='utf-8') as err:
        result = subprocess.run([str(exe), str(ROOT.parent), '100000', str(side), str(mode)],
                                stdout=out, stderr=err, cwd=ROOT)
    if result.returncode:
        raise AssertionError((suffix, result.returncode, error.read_text()))
    report = json.loads(output.read_text())
    print(f"PASS {suffix} charts={report['charts']} frames={report['frames']}", flush=True)
    return report

with ThreadPoolExecutor(max_workers=2) as pool:
    configurations = list(pool.map(run, [(0, 1), (1, 1), (0, 0), (1, 0)]))
report = {'mode': 'offline, original catalog, every 60Hz snapshot, no resource conversion',
          'configurations': configurations,
          'total_chart_runs': sum(c['charts'] for c in configurations),
          'frames': sum(c['frames'] for c in configurations),
          'limitations': ['local components only, as original 10158F20; opponent has head movie only',
                          'offline session has no replay collection association',
                          'special overlays remain explicit original effect queue requests',
                          'host audit does not verify ARM64 pixels or AFP free-list equivalence']}
(ROOT / '.build/stage-long-presentation-audit.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(json.dumps({key: value for key, value in report.items() if key != 'configurations'}))
