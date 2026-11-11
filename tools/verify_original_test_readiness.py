"""Compare TEST wait decisions against bounded original x86 instructions, offline."""
import argparse
import hashlib
import itertools
import json
import random
import struct
import subprocess
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['game-root', 'dependencies', 'executable', 'report']:
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if args.report.exists():
        raise ValueError('Report already exists')
    sys.path.insert(0, str(args.dependencies))
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP

    raw = (args.game_root / 'modules/reflecbeat.dll').read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    assert digest == 'ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
    pe = pefile.PE(data=raw)
    base = pe.OPTIONAL_HEADER.ImageBase
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    machine.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    machine.mem_write(base, pe.get_memory_mapped_image())
    arena, stack, end = 0x7000000, 0x8000000, 0x9000000
    for address in [arena, stack, end]:
        machine.mem_map(address, 65536)

    def put(address, value):
        machine.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def read(address):
        return struct.unpack('<I', machine.mem_read(address, 4))[0]

    calls = []
    status = 0

    def hook(uc, address, size, user):
        if address not in [end + 16, end + 32]:
            return
        calls.append('B' if address == end + 16 else 'S')
        # Begin's return is ignored by the original. Supply a poison value.
        uc.reg_write(UC_X86_REG_EAX, (0xfedcba98 if address == end + 16 else status) & 0xffffffff)
        sp = uc.reg_read(UC_X86_REG_ESP)
        uc.reg_write(UC_X86_REG_EIP, read(sp))
        uc.reg_write(UC_X86_REG_ESP, sp + 4)

    machine.hook_add(UC_HOOK_CODE, hook)
    # IAT slots observed in original 100D5710; only external EA3 operations
    # are intercepted. The status classifier executes its original code.
    put(0x10189588, end + 16)
    put(0x10189584, end + 32)
    rng = random.Random(0x100D5710)
    statuses = list(range(-128, 129)) + [-2147483648, 2147483647]
    statuses += [rng.randint(-2147483648, 2147483647) for _ in range(1024)]
    ticks = [0, 1, -1, 2, -2147483648, 2147483647]
    inputs, expected = [], []
    for tick, status in itertools.product(ticks, statuses):
        calls.clear()
        initial = bytearray(rng.getrandbits(8) for _ in range(256))
        struct.pack_into('<i', initial, 180, tick)
        # Keep a known different pending handler to distinguish a queued write.
        struct.pack_into('<I', initial, 160, 0x12345678)
        machine.mem_write(arena, bytes(initial))
        sp = stack + 60000
        put(sp, end)
        put(sp + 4, 0xaabbccdd)
        machine.reg_write(UC_X86_REG_ESP, sp)
        machine.reg_write(UC_X86_REG_ECX, arena)
        machine.emu_start(0x100D5710, end, count=1000)
        assert machine.reg_read(UC_X86_REG_EIP) == end
        assert machine.reg_read(UC_X86_REG_ESP) == sp + 8
        queued = read(arena + 160) == 0x100D5600
        expected_memory = bytearray(initial)
        if queued:
            struct.pack_into('<I', expected_memory, 160, 0x100D5600)
            struct.pack_into('<I', expected_memory, 164, 0)
            struct.pack_into('<I', expected_memory, 172, 0)
            expected_memory[176] = 1
        assert bytes(machine.mem_read(arena, 256)) == bytes(expected_memory)
        # Also execute the complete original classifier independently.
        put(sp, end)
        machine.reg_write(UC_X86_REG_ESP, sp)
        machine.reg_write(UC_X86_REG_EAX, status & 0xffffffff)
        machine.emu_start(0x100A1610, end, count=1000)
        assert machine.reg_read(UC_X86_REG_EIP) == end
        normalized = struct.unpack('<i', struct.pack('<I', machine.reg_read(UC_X86_REG_EAX)))[0]
        inputs.append(f'{tick} {status}')
        expected.append(f'{normalized} {int(queued)} {"".join(calls)}')
    actual = subprocess.check_output([str(args.executable), '--batch'],
                                    input='\n'.join(inputs) + '\n', text=True).splitlines()
    assert len(actual) == len(expected)
    for line, actual_line, expected_line in zip(inputs, actual, expected):
        assert actual_line == expected_line, (line, actual_line, expected_line)
    report = {
        'status': 'passed', 'cases': len(inputs), 'dll_sha256': digest,
        'functions': ['0x100D5710', '0x100A1610'],
        'verified': ['status mapping', 'EA3 boundary call order', 'main-menu queue decision',
                     'original scene field writes and preservation'],
        'limits': 'External EA3 begin/status calls are captured and supplied synthetic values. '
                  'No live EA3, hardware, network, TEST subscene or complete bootstrap executes. '
                  'Native result is a decision; original scene ownership and transition dispatch '
                  'must be connected separately. Error status queues the menu, not successful readiness.'
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS', len(inputs), 'original TEST wait decisions, call traces and status mappings')


if __name__ == '__main__':
    main()
