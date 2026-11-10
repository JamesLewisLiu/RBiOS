"""Compare sound selection with isolated original x86 instructions and SD9 headers."""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import struct
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--dependencies', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if args.report.exists():
        raise ValueError('Report already exists')
    for folder in ['data', 'modules']:
        if args.report.resolve().is_relative_to((args.game_root / folder).resolve()):
            raise ValueError('Report must be outside original resources')
    sys.path.insert(0, str(args.dependencies.resolve()))
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS
    data = (args.game_root / 'modules/reflecbeat.dll').read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    assert digest == 'ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
    pe = pefile.PE(data=data)
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    base = pe.OPTIONAL_HEADER.ImageBase
    u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    u.mem_write(base, pe.get_memory_mapped_image())
    arena, stack, end = 0x7000000, 0x8000000, 0x9000000
    for address in [arena, stack, end]:
        u.mem_map(address, 65536)
    note, chart, manager, vtable = [arena + i * 1024 for i in range(4)]

    def word(n): return struct.pack('<I', n & 0xffffffff)
    def put(address, n): u.mem_write(address, word(n))

    put(note, vtable)
    put(note + 160, chart)
    put(vtable + 152, 0x10158910)
    put(0x12479294, manager)
    inputs, expected = [], []
    # Native 1015E620 calls original note+152, which calls original 10163A10.
    # Every isolated selection starts with mature cooldowns and empty queues.
    for row in itertools.product([-1, 0, 1, 2], range(5), [-1, 0, 1, 2, 3, 4, 6],
                                 range(-1, 14), [-1, 0, 1, 2], [0, 1]):
        side, kind, grade, reason, phase, miss = row
        put(chart + 328, side); put(chart + 288, kind); put(note + 372, phase)
        u.mem_write(manager, bytes(104))
        for i, n in enumerate([10192, 11312, 12432, 10192, 15072 if miss else 9472]):
            put(manager + 8 + i * 4, n)
        put(manager + 48, 14912); put(manager + 52, 14992); put(manager + 56, 127)
        for offset in [60, 72, 80, 88]: put(manager + offset, 9472)
        for offset in [68, 76, 84, 92]: put(manager + offset, 3)
        sp = stack + 60000
        u.mem_write(sp, word(end) + word(note) + word(grade) + word(reason))
        u.reg_write(UC_X86_REG_ESP, sp); u.reg_write(UC_X86_REG_EFLAGS, 2)
        u.emu_start(0x1015e620, end, count=2000)
        assert u.reg_read(UC_X86_REG_EIP) == end
        ids = [struct.unpack('<I', u.mem_read(manager + offset, 4))[0]
               for offset in [60, 72, 80, 88]]
        selected = [n for n in ids if n != 9472]
        assert len(selected) <= 1
        inputs.append(' '.join(map(str, row)))
        expected.append(str(selected[0] if selected else 9472))
    actual = subprocess.check_output([str(args.executable), '--batch'],
                                     input='\n'.join(inputs) + '\n', text=True).splitlines()
    assert len(actual) == len(expected)
    for row, a, e in zip(inputs, actual, expected):
        assert a == e, (row, a, e)
    resources = []
    for suffix, sound_id in [('just', 10192), ('great', 11312), ('good', 12432),
                             ('long_end', 14752), ('long_on', 14832), ('bigbang', 14912),
                             ('damage', 15072)]:
        name = 'sys_shot_damage' if suffix == 'damage' else 'sys_shot_default1_' + suffix
        path = args.game_root / 'data/sound/sys_sd_rb6' / (name + '.sd9')
        raw = path.read_bytes()
        assert raw[:4] == b'SD9\0' and raw[32:36] == b'RIFF'
        assert struct.unpack_from('<H', raw, 30)[0] * 80 + 8192 == sound_id
        assert struct.unpack_from('<HHI', raw, 52) == (1, 2, 44100)
        assert struct.unpack_from('<H', raw, 66)[0] == 16
        resources.append({'path': str(path.relative_to(args.game_root)), 'id': sound_id,
                          'sha256': hashlib.sha256(raw).hexdigest(), 'bytes': len(raw)})
    report = {'status': 'passed', 'selection_cases': len(inputs), 'dll_sha256': digest,
              'resources': resources, 'limits': 'Selection oracle only; no device audio capture.'}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS', len(inputs), 'original sound selections and', len(resources), 'SD9 headers')


if __name__ == '__main__':
    main()
