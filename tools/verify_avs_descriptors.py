"""Compare the recovered AVS descriptor lifecycle with original x86, offline."""
import argparse, hashlib, json, random, struct, subprocess, sys
from pathlib import Path

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['dll', 'dependencies', 'executable', 'report']:
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    if a.report.exists(): raise ValueError('Report already exists')
    sys.path.insert(0, str(a.dependencies))
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
    raw = a.dll.read_bytes(); digest = hashlib.sha256(raw).hexdigest()
    assert digest == 'd062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe = pefile.PE(data=raw); base = pe.OPTIONAL_HEADER.ImageBase
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    u.mem_write(base, pe.get_memory_mapped_image())
    arena, stack, end = 0x7000000, 0x8000000, 0x9000000
    for address in [arena, stack, end]: u.mem_map(address, 0x10000)
    def put(address, value): u.mem_write(address, struct.pack('<I', value & 0xffffffff))
    def get(address): return struct.unpack('<I', u.mem_read(address, 4))[0]
    depth = 0
    callbacks = []
    def hook(uc, address, size, data):
        nonlocal depth
        sp = uc.reg_read(UC_X86_REG_ESP)
        result = 0
        if address == 0x10011090:
            uc.mem_write(get(sp+4), bytes([get(sp+8) & 255]) * get(sp+12))
        elif address == 0x10022940: depth += 1
        elif address == 0x100228b0: depth -= 1; assert depth >= 0
        elif address == end + 16:
            assert depth == 0, 'Callback must execute after unlock'
            result = get(sp+4)
            callbacks.append(result)
        elif address not in [0x1000a950, 0x1000a980]: return
        uc.reg_write(UC_X86_REG_EAX, result)
        uc.reg_write(UC_X86_REG_EIP, get(sp)); uc.reg_write(UC_X86_REG_ESP, sp+4)
    u.hook_add(UC_HOOK_CODE, hook)
    def run(address, *args):
        sp = stack + 60000; put(sp, end)
        for i, value in enumerate(args): put(sp + 4 + i*4, value)
        u.reg_write(UC_X86_REG_ESP, sp)
        u.emu_start(address, end, count=100000)
        assert u.reg_read(UC_X86_REG_EIP) == end and depth == 0
        return u.reg_read(UC_X86_REG_EAX)
    inputs, expected = [], []
    rng = random.Random(103)
    for capacity in [1, 2, 17, 808]:
        u.mem_write(arena, bytes(65536))
        assert run(0x10015110, arena, capacity) == arena + 24*(capacity+4)
        inputs.append(f'I {capacity} 0 0'); expected.append(str(capacity))
        # Real lifecycle instructions run with a supplied synchronization handle.
        # OS lock primitives are trace hooks, never Windows/AVS execution.
        put(0x1007b5d0, 1)
        live, stale = [], []
        for index in range(1600):
            op = rng.choice(['A', 'A', 'L', 'R', 'S']) if live else 'A'
            if op == 'A':
                category, callback, payload = rng.randrange(1,64), rng.randrange(2), rng.randrange(100000)
                result = run(0x100148f0, category, end+16 if callback else 0, payload)
                inputs.append(f'A {category} {callback} {payload}'); expected.append(str(result))
                if result: live.append(result)
            else:
                token = rng.choice(stale) if stale and rng.randrange(5)==0 else rng.choice(live)
                if op == 'L':
                    put(arena+60000, 0x12345678)
                    result = run(0x100146c0, token, arena+60000)
                    inputs.append(f'L {token} 0 0'); expected.append(f'{result} {get(arena+60000)}')
                elif op == 'S':
                    callback, payload = rng.randrange(2), rng.randrange(100000)
                    result = run(0x10015490, token, end+16 if callback else 0, payload)
                    inputs.append(f'S {token} {callback} {payload}'); expected.append(str(result))
                else:
                    result = run(0x10014700, token)
                    inputs.append(f'R {token} 0 0'); expected.append(str(result))
                    if token in live: live.remove(token); stale.append(token)
        # Original lists and tokens are indirectly exercised by random middle,
        # head and tail removal, exhaustion, and reuse through continued allocation.
    cleanup_commands = 0
    for case in range(64):
        capacity = 64
        u.mem_write(arena, bytes(65536)); run(0x10015110, arena, capacity)
        put(0x1007b5d0, 1)
        inputs.append('I 64 0 0'); expected.append('64')
        # All reserved owners bind explicit callbacks before cleanup of group 0.
        for token in [0x180000, 0x180001, 0x180002, 0x40003]:
            result = run(0x10015490, token, end+16, token)
            inputs.append(f'S {token} 1 {token}'); expected.append(str(result))
        tokens = []
        for index in range(48):
            group = rng.randrange(4)
            result = run(0x10015040, group)
            inputs.append(f'B {group} 0 0'); expected.append(str(result))
            category, value = rng.randrange(1, 64), index+1
            token = run(0x100148f0, category, end+16, value)
            inputs.append(f'A {category} 1 {value}'); expected.append(str(token)); tokens.append(token)
        for index in range(12):
            callbacks.clear()
            if index % 3:
                category = rng.randrange(1,64)
                run(0x100143c0, category); inputs.append(f'C {category} 0 0')
            else:
                group = rng.randrange(4)
                run(0x10014d40, group); inputs.append(f'G {group} 0 0')
            expected.append(' '.join(map(str, [len(callbacks)] + callbacks)))
            cleanup_commands += 1
            for token in rng.sample(tokens, 5):
                put(arena+60000, 0x12345678)
                result = run(0x100146c0, token, arena+60000)
                inputs.append(f'L {token} 0 0'); expected.append(f'{result} {get(arena+60000)}')
            category = rng.randrange(1,64)
            result = run(0x100148f0, category, end+16, index+123)
            inputs.append(f'A {category} 1 {index+123}'); expected.append(str(result))
    actual = subprocess.check_output([str(a.executable), '--batch'], input='\n'.join(inputs)+'\n', text=True).splitlines()
    assert len(actual) == len(expected)
    for command, x, y in zip(inputs, actual, expected): assert x == y, (command, x, y)
    report = {'status':'passed','commands':len(inputs),'dll_sha256':digest,
              'cleanup_commands':cleanup_commands,
              'functions':['10015110','100148F0','10014700','100146C0','10015490','10015040','100143C0','10014D40'],
              'limits':'Memory fill/log/lock boundaries hooked; no OS mutex pool, network or game process. Callback order and unlock ordering checked, including deferred flags and retained tokens during cleanup.'}
    a.report.parent.mkdir(parents=True, exist_ok=True)
    a.report.write_text(json.dumps(report,indent=2)+'\n', encoding='utf-8')
    print('PASS',len(inputs),'original AVS descriptor commands')
if __name__ == '__main__': main()
