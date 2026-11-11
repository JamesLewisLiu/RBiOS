"""Compare alarm records and dispatch against original AVS x86 instructions, offline."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import sys


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
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP
    raw = (args.game_root / 'modules/libavs-win32.dll').read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    assert digest == 'd062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe = pefile.PE(data=raw)
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    base = pe.OPTIONAL_HEADER.ImageBase
    u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    u.mem_write(base, pe.get_memory_mapped_image())
    arena, stack, stubs, end = 0x7000000, 0x8000000, 0x9000000, 0x9010000
    for addr in [arena, stack, stubs, end]:
        u.mem_map(addr, 65536)
    def w32(addr, value): u.mem_write(addr, struct.pack('<I', value & 0xffffffff))
    def r32(addr): return struct.unpack('<I', u.mem_read(addr, 4))[0]
    def r64(addr): return struct.unpack('<Q', u.mem_read(addr, 8))[0]
    imports = {}
    for entry in pe.DIRECTORY_ENTRY_IMPORT:
        for imp in entry.imports:
            addr = stubs + len(imports) * 16
            imports[addr] = (imp.name or b'ordinal').decode()
            w32(imp.address, addr)
    callback = stubs + 0xf000
    tick, calls, keep = 0, [], {}
    def hook(uc, address, size, context):
        if address == end:
            uc.emu_stop(); return
        if address not in imports and address != callback: return
        sp = uc.reg_read(UC_X86_REG_ESP)
        if address == callback:
            identity = r32(sp + 4)
            calls.append((identity, r64(sp + 8)))
            result, count = keep[identity], 0
        else:
            name = imports[address]
            counts = {'WaitForSingleObject': 2, 'ReleaseMutex': 1, 'ReleaseSemaphore': 3,
                      'EnterCriticalSection': 1, 'LeaveCriticalSection': 1, 'timeGetTime': 0}
            if name not in counts: raise AssertionError('Unexpected import: ' + name)
            result, count = (tick if name == 'timeGetTime' else 1), counts[name]
        uc.reg_write(UC_X86_REG_EAX, int(result))
        uc.reg_write(UC_X86_REG_ESP, sp + 4 + count * 4)
        uc.reg_write(UC_X86_REG_EIP, r32(sp))
    u.hook_add(UC_HOOK_CODE, hook)
    def call(address, arguments=(), eax=0, edx=0):
        sp = stack + 0xf000
        u.mem_write(sp, struct.pack('<' + 'I' * (len(arguments) + 1), end, *arguments))
        u.reg_write(UC_X86_REG_ESP, sp)
        u.reg_write(UC_X86_REG_EAX, eax); u.reg_write(UC_X86_REG_EDX, edx)
        u.emu_start(address, end, count=200000)
        assert u.reg_read(UC_X86_REG_EIP) == end
        return u.reg_read(UC_X86_REG_EAX)
    call(0x10040bc0, eax=arena, edx=4)
    tokens = {}
    def chain(head):
        values = []
        while head:
            assert head >= arena and (head - arena) % 40 == 0 and len(values) < 4
            values.append((head - arena) // 40); head = r32(head + 20)
        return values
    def snapshot(result):
        queue, free = chain(r32(0x1007d02c)), chain(r32(0x1007d028))
        values = [result, r64(0x1007d020), r32(0x1007d038), r32(0x1007d034), len(queue), *queue, len(free), *free]
        for index in range(4):
            addr = arena + 40 * index
            flags = bytes(u.mem_read(addr + 36, 3))
            values += [r64(addr), r64(addr + 8), int(index not in free), flags[1], flags[2], flags[0]]
        values.append(len(calls))
        for identity, time in calls: values += [identity, time]
        return ' '.join(map(str, values))
    operations = [('T',0,100,0), ('C',1,0,1), ('C',2,0,0), ('S',1,10,0), ('S',2,10,0),
                  ('S',1,99,1), ('T',0,110,0), ('P',0,0,0), ('P',0,0,0), ('D',2,0,0),
                  ('C',3,1,1), ('T',0,120,0), ('P',0,0,0), ('P',0,0,0)]
    # Keep a single surviving public alarm; random reschedule/stop/poll drives
    # wraparound and signed modular ordering without guessing descriptor ABI.
    rng = random.Random(0x40a60)
    for _ in range(240):
        operations.append(rng.choice([('S',1,rng.choice([0,1,33,2**32,2**63,2**64-1]),rng.randrange(2)),
                                      ('T',0,rng.randrange(2**32),0), ('P',0,0,0)]))
    expected = []
    for op, identity, a, b in operations:
        calls.clear(); result = 0
        if op == 'T': tick = a
        elif op == 'C':
            keep[identity] = b
            ptr = call(0x100405e0, (callback, identity, a))
            result = int(bool(ptr))
            if ptr:
                tokens[identity] = ptr
                if a: call(0x10040490, (ptr,10,0,0))
        elif op == 'S':
            if not a and b: result = -2147090410
            elif not a: result = call(0x10040450, (tokens[identity],))
            else: result = call(0x10040490, (tokens[identity],a & 0xffffffff,a >> 32,b))
        elif op == 'D': call(0x10040570, (tokens[identity],))
        elif op == 'P':
            u.mem_write(0x1007d03d,b'\0'); call(0x10040a60)
            lo = call(0x10040d00); hi = u.reg_read(UC_X86_REG_EDX)
            head = call(0x100409c0,(lo,hi))
            u.mem_write(0x1007d03d,b'\1')
            call(0x10040960,(0,lo,hi),eax=head)
            result = len(calls)
        expected.append(snapshot(result))
    text = ''.join(f'{op} {i} {a} {b}\n' for op,i,a,b in operations)
    process = subprocess.run([str(args.executable),'--batch'],input=text,text=True,capture_output=True,check=True)
    actual = process.stdout.splitlines()
    assert len(actual) == len(expected)
    for index,(got,want) in enumerate(zip(actual,expected)):
        assert got == want, f'case {index}: {operations[index]}\nnative {got}\nsource {want}'
    report = {'sha256':digest,'cases':len(expected),'passed':True,
              'source_functions':['10040BC0','100405E0','10040490','10040450','10040570','10040A60','10040D00','100409C0','10040960'],
              'stubs':'Win32 synchronization/time imports and caller callback only; no AVS alarm function stubbed',
              'scope':'finite record state, queue/free ordering, deadlines, callbacks, wrap; not worker admission or public descriptor wrappers'}
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))


if __name__ == '__main__': main()
