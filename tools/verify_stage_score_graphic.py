"""Compare stage score/combo draws against isolated original x86 instructions.

No original game process, resource conversion, or network access is involved.
Named AFP positions and renderer virtuals are explicit capture fixtures.
"""
import argparse
import importlib.util
import itertools
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--analysis-root', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    args.report.resolve().relative_to((args.analysis_root / '.build').resolve())
    if args.report.exists():
        raise ValueError('Report already exists; select a new evidence report path')
    source = args.analysis_root / 'tools/original_score_fixture.py'
    spec = importlib.util.spec_from_file_location('original_score_fixture', source)
    f = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(f)
    from unicorn import UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
    u = f.u
    actor, opponent, scene, array, movie, factory, sprite, table = [f.scratch + 0x5000 + i * 0x200 for i in range(8)]
    positions = {'score_as_you': (384, 624), 'score_as_rival': (384, 516),
                 'combo_as_you': (384, 737), 'combo_as_rival': (384, 300),
                 'combo_you': (384, 807), 'combo_rival': (384, 269)}
    nodes = {name: f.scratch + 0x7000 + i * 0x100 for i, name in enumerate(positions)}
    coordinates = {nodes[name]: point for name, point in positions.items()}
    f.put(f.manager + 16, opponent)
    f.put(0x12479298, scene); f.put(scene + 160, array); f.put(scene + 164, 1); f.put(array, movie)
    f.put(sprite, table)
    setters = {f.end + 0x800: 'position', f.end + 0x820: 'anchor', f.end + 0x840: 'alignment'}
    for address in setters: u.mem_write(address, b'\xc2\x08\x00')
    f.put(table + 28, f.end + 0x800); f.put(table + 44, f.end + 0x820); f.put(table + 36, f.end + 0x840)
    captured = []
    current = {}

    def integer(address):
        return int.from_bytes(u.mem_read(address, 4), 'little')

    def string(address):
        return bytes(u.mem_read(address, 100)).split(b'\0')[0].decode('ascii')

    def finish(value, argument_bytes):
        sp = u.reg_read(UC_X86_REG_ESP)
        u.reg_write(UC_X86_REG_EAX, value)
        u.reg_write(UC_X86_REG_EIP, integer(sp))
        u.reg_write(UC_X86_REG_ESP, sp + 4 + argument_bytes)

    def hook(_uc, address, _size, _data):
        sp = u.reg_read(UC_X86_REG_ESP)
        if address == 0x1002AB70:
            finish(factory, 0)
        elif address == 0x10029920:
            finish(nodes[string(integer(sp + 4))], 4)
        elif address == 0x10029E70:
            x, y = coordinates[u.reg_read(UC_X86_REG_ECX)]
            f.put(integer(sp + 4), x); f.put(integer(sp + 8), y); finish(0, 8)
        elif address == 0x1002AC30:
            current.clear()
            current.update(name=string(integer(sp + 4)), layer=integer(sp + 20))
            finish(sprite, 24)
        elif address in setters:
            kind = setters[address]
            values = [integer(sp + 4), integer(sp + 8)]
            if kind == 'position':
                current['position'] = values
                captured.append(dict(current))
            else:
                current[kind] = values
            finish(sprite, 8)

    u.hook_add(UC_HOOK_CODE, hook)
    cases, expected = [], []
    boundaries = [0, 1, 9, 10, 99, 100, 999, 1000, 9999, 10000, 99999, 100000,
                  12345, 987654321, 2147483647]
    for number, color, yellow0, yellow1 in itertools.product(boundaries, range(2), range(2), range(2)):
        row = [number, boundaries[-1 - boundaries.index(number)], number, (number * 7) % 100001, yellow0, yellow1, color]
        u.mem_write(actor, bytes(0x200)); u.mem_write(opponent, bytes(0x200)); u.mem_write(f.player, bytes(132))
        u.mem_write(actor + 21, b'\x01'); f.put(actor + 136, color); f.put(actor + 140, 1 - color)
        f.put(f.battle + 8, row[0]); f.put(f.battle + 12, row[1])
        f.put(f.player + 60, row[2]); f.put(opponent + 60, row[3]); f.put(f.player + 16, 0 if yellow0 else 1)
        u.mem_write(opponent + 104, bytes([yellow1]))
        captured.clear(); f.call(0x10152600, [actor])
        # Assert the original capture's loop boundary independently of native output.
        families = ['score_as_small_' + ('blue_' if color else 'red_'),
                    'score_as_small_' + ('red_' if color else 'blue_'),
                    'combo_big_as_', 'combo_small_as_']
        for value, family in zip(row[:4], families):
            count = sum(item['name'].startswith(family) for item in captured)
            assert count == min(4, len(str(value))), (row, family, count)
        normalized = []
        for item in captured:
            if 'anchor' in item:
                name = item['name']
                size = (44, 56) if name.startswith('score_') else ((84, 106) if 'big' in name else (28, 36))
                anchor = [item['anchor'][i] / size[i] for i in range(2)]
            else:
                assert item['alignment'] == [3, 3]
                anchor = [.5, .5]
            normalized.append((item['name'], *item['position'], *anchor, item['layer']))
        cases.append(row); expected.append(normalized)
    output = subprocess.check_output([str(args.executable), '--batch'], input=('\n'.join(' '.join(map(str, row)) for row in cases) + '\n').encode()).decode().splitlines()
    cursor = 0
    for row, expected_draws in zip(cases, expected):
        count = int(output[cursor]); cursor += 1
        assert count == len(expected_draws), (row, count, len(expected_draws))
        for wanted in expected_draws:
            fields = output[cursor].split(); cursor += 1
            actual = (fields[0], *map(float, fields[1:]))
            assert actual == wanted, (row, actual, wanted)
    assert cursor == len(output)
    report = {'status': 'passed', 'entry': '0x10152600', 'dll_sha256': f.digest,
              'cases': len(cases), 'draws': sum(map(len, expected)), 'named_positions': positions,
              'original_maximum_digits_per_counter': 4,
              'limitations': ['Explicit AFP position fixture; original AFP VM not executed',
                              'Draw virtuals captured; no GPU or original engine pixel comparison',
                              'Ordinary score mode only; no life/course branch or animation smoothing']}
    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS', report['cases'], 'original stage score cases,', report['draws'], 'draws')


if __name__ == '__main__':
    main()
