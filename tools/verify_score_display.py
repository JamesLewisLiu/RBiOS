"""Compare the portable display counter against isolated original x86 instructions."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-root', type=Path, required=True)
    p.add_argument('--dependencies', type=Path, required=True)
    p.add_argument('--executable', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    sys.path.insert(0, str(a.dependencies.resolve()))
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP
    data = (a.game_root / 'modules/reflecbeat.dll').read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    assert digest == 'ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
    pe = pefile.PE(data=data)
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    u.mem_map(pe.OPTIONAL_HEADER.ImageBase, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    u.mem_write(pe.OPTIONAL_HEADER.ImageBase, pe.get_memory_mapped_image())
    memory, stack, end = 0x7000000, 0x8000000, 0x9000000
    for address in (memory, stack, end):
        u.mem_map(address, 65536)
    rng = random.Random(2019121100)
    boundary = [-2147483648, -2147483647, -1, 0, 1, 2, 9998, 9999, 2147483646, 2147483647]
    cases = [(x, y, y, x, a, b) for x in boundary for y in boundary for a in (0, 1) for b in (0, 1)]
    cases += [tuple(rng.randint(-2147483648, 2147483647) for _ in range(4)) +
              (rng.randrange(2), rng.randrange(2)) for _ in range(2000)]
    expected = []
    for case in cases:
        raw = bytearray(rng.randbytes(172))
        struct.pack_into('<4i2B', raw, 0, *case)
        u.mem_write(memory, bytes(raw))
        u.mem_write(stack + 60000, struct.pack('<I', end))
        u.reg_write(UC_X86_REG_EAX, memory)
        u.reg_write(UC_X86_REG_ESP, stack + 60000)
        u.emu_start(0x10155460, end, count=200)
        assert u.reg_read(UC_X86_REG_EIP) == end
        after = bytes(u.mem_read(memory, 172))
        assert after[:8] == raw[:8] and after[18:] == raw[18:]
        expected.append(struct.unpack_from('<2i2B', after, 8))
    result = subprocess.run([str(a.executable.resolve()), '--wire'],
                            input=''.join(' '.join(map(str, c)) + '\n' for c in cases),
                            capture_output=True, text=True, check=True)
    actual = [tuple(map(int, line.split())) for line in result.stdout.splitlines()]
    assert len(actual) == len(expected)
    for index, (x, y) in enumerate(zip(actual, expected)):
        assert x == y, (index, cases[index], x, y)
    output = a.output.resolve()
    for directory in ('data', 'modules', 'modules - Copy'):
        if output.is_relative_to((a.game_root / directory).resolve()):
            raise ValueError('Output must be outside original resource directories')
    report = {'status': 'passed', 'cases': len(cases), 'entry': '0x10155460',
              'dll_sha256': digest, 'untouched_bytes_checked': True,
              'scope': 'Isolated original integer display-counter update; no whole-game scheduling trace'}
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('x', encoding='utf-8') as f:
        json.dump(report, f, indent=2)
        f.write('\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
