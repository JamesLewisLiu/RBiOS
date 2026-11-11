"""Compare XML metadata conversion against original AVS instructions, without stubs."""
import argparse, hashlib, json, random, struct, subprocess, sys
from pathlib import Path

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('game-root', 'dependencies', 'executable', 'report'): p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    if a.report.exists(): raise ValueError('Report already exists')
    for name in ('data', 'modules', 'prop'):
        if a.report.resolve().is_relative_to((a.game_root / name).resolve()): raise ValueError('Report overlaps original assets')
    sys.path.insert(0, str(a.dependencies.resolve()))
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP
    raw = (a.game_root / 'modules/libavs-win32.dll').read_bytes(); digest = hashlib.sha256(raw).hexdigest()
    assert digest == 'd062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe = pefile.PE(data=raw); base = pe.OPTIONAL_HEADER.ImageBase; image = pe.get_memory_mapped_image()
    u = Uc(UC_ARCH_X86, UC_MODE_32); u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    u.mem_write(base, image); u.mem_map(0x7000000, 0x30000)
    cases = ['', '0', '-0', '00', '0x', '0X', '+0', '-1', '08', '09', '18', '1A', 'A', 'f',
             ' 0', '  ', '- ', '0 ', '0 z', '2 trailing', '2z', '\t8', '0xFF', '0xff',
             '4294967295', '4294967296', '18446744073709551615', '18446744073709551616',
             '01777777777777777777777', '0xffffffffffffffff', '0x10000000000000000']
    for i in range(65):
        pointer, value = struct.unpack_from('<II', image, 0x78080 + i * 8)
        name = image[pointer-base:image.index(b'\0', pointer-base)].decode('ascii')
        cases += [name, name.upper(), ' ' + name, name + ' ']
    rng = random.Random(0x24af0)
    for _ in range(512):
        value = rng.randrange(1 << rng.choice([8, 16, 32, 64]))
        cases.append(rng.choice(['', '-', '+', ' ']) + rng.choice([str(value), hex(value), '0' + oct(value)[2:]]) + rng.choice(['', ' suffix', 'z']))
    def call(address, *args):
        sp = 0x702f000; u.mem_write(sp, struct.pack('<' + 'I' * (len(args) + 1), 0x7010020, *args))
        u.reg_write(UC_X86_REG_ESP, sp); u.emu_start(address, 0x7010020, count=100000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x7010020
        return u.reg_read(UC_X86_REG_EAX)
    expected = []
    for text in cases:
        u.mem_write(0x7000000, text.encode() + b'\0')
        kind = call(0x1002d690, 0x7000000)
        u.mem_write(0x7001000, bytes(8))
        status = call(0x10024af0, 0x7000000, 0x7001000, 0, 0, 0, 0xffffffff, 0)
        status = struct.unpack('<i', struct.pack('<I', status))[0]
        value = struct.unpack('<Q', u.mem_read(0x7001000, 8))[0] if status >= 0 else 0
        expected.append(f'{kind} {status} {value}')
    got = subprocess.check_output([str(a.executable), '--batch'], input='\n'.join(cases) + '\n', text=True).splitlines()
    assert len(got) == len(expected)
    for text, actual, wanted in zip(cases, got, expected): assert actual == wanted, (text, actual, wanted)
    a.report.parent.mkdir(parents=True, exist_ok=True)
    a.report.write_text(json.dumps({'status': 'passed', 'cases': len(cases), 'dll_sha256': digest,
        'addresses': ['0x1002D690', '0x10024E30', '0x10078080', '0x10024AF0'],
        'compared': '65 type aliases, case/whitespace mismatch, XML size/count values and exact helper return status',
        'limits': 'Original instructions execute without stubs. Only XML metadata helpers are recovered; XML grammar, node content scalar conversion, encoding and serialization are not covered.'}, indent=2) + '\n')
    print(f'PASS {len(cases)} original XML metadata conversion comparisons')
if __name__ == '__main__': main()
