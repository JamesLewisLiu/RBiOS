"""Compare the recovered filesystem boot phase with original AVS x86 instructions."""
import argparse, hashlib, json, random, struct, subprocess, sys
from pathlib import Path

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-root', type=Path, required=True)
    p.add_argument('--dependencies', type=Path, required=True)
    p.add_argument('--executable', type=Path, required=True)
    p.add_argument('--report', type=Path, required=True)
    a = p.parse_args()
    if a.report.exists(): raise ValueError('Report already exists')
    for name in ('data', 'modules'):
        if a.report.resolve().is_relative_to((a.game_root/name).resolve()): raise ValueError('Report must be outside original assets')
    sys.path.insert(0, str(a.dependencies.resolve()))
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP
    raw = (a.game_root/'modules/libavs-win32.dll').read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    assert digest == 'd062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe = pefile.PE(data=raw); base = pe.OPTIONAL_HEADER.ImageBase
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
    u.mem_write(base, pe.get_memory_mapped_image())
    config, stack, end = 0x7000000, 0x8000000, 0x9000000
    for address in (config, stack, end): u.mem_map(address, 65536)
    def string(address):
        value = bytearray()
        for offset in range(4096):
            c = u.mem_read(address+offset, 1)[0]
            if c == 0: return bytes(value)
            value.append(c)
        raise AssertionError('Unterminated oracle string')
    def hx(address): return string(address).hex() or '-'
    factories = {}
    for i in range(6):
        name, address = struct.unpack('<II', u.mem_read(0x10068abc+i*8, 8))
        factories[address] = (i, string(name).decode())
    state = {}
    def result(normal=1):
        state['calls'] += 1
        return state['response'] if state['calls'] == state['failedCall'] else normal
    def hook(u, address, size, data):
        if address not in factories and address not in (0x1000a950, 0x1000a980, 0x1000a780, 0x10029080, 0x1001b2d0, 0x1001b020, 0x1001af50): return
        sp = u.reg_read(UC_X86_REG_ESP)
        args = struct.unpack('<6I', u.mem_read(sp, 24))
        value = 1
        if address in factories:
            i, name = factories[address]
            state['trace'].append(f'F:{name}:{address}')
            value = 0x50000000 + i*16
        elif address == 0x10029080:
            state['trace'].append(f'A:{args[1]}'); value = result(); state['last'] = value; state['stage'] = 1
        elif address == 0x1001b2d0:
            state['trace'].append('M:' + ':'.join(hx(v) for v in args[1:5])); value = result(); state['last'] = value; state['stage'] = 2
        elif address == 0x1001b020:
            state['trace'].append('O:' + hx(args[1])); value = result(17); state['last'] = value; state['stage'] = 3
        elif address == 0x1001af50:
            state['trace'].append(f'T:{args[1]}:{hx(args[2])}'); value = result()
        elif address == 0x1000a780:
            state['error'] = f"E:{state['stage']}:{state['last']}"
            u.emu_stop(); return
        u.reg_write(UC_X86_REG_EAX, value & 0xffffffff)
        u.reg_write(UC_X86_REG_ESP, sp+4); u.reg_write(UC_X86_REG_EIP, args[0])
    u.hook_add(UC_HOOK_CODE, hook)
    rng = random.Random(0x13e00); rows = []; expected = []
    for devices in range(8):
        for table in (False, True):
            for failure in range(12):
                for response in (-2147483648, -1, 0, 1, 2147483647):
                    image = bytearray(2528)
                    for index in range(3):
                        offset = 138 + index*531
                        fs = (f'fs{index}' + 'f'*rng.randrange(13)).encode()
                        device = (f'device/{index}/'+'d'*rng.choice((0, 12, 246))).encode() if devices & (1<<index) else b''
                        options = ('ro,flags='+str(rng.randrange(65536))).encode()
                        for pos, value in ((offset, fs), (offset+17, device), (offset+274, options)):
                            image[pos:pos+len(value)] = value
                    if table:
                        value = b'/config/mounttable.xml'; image[1731:1731+len(value)] = value
                    state.clear(); state.update(trace=[], calls=0, failedCall=failure, response=response, stage=0, last=0)
                    u.mem_write(config, bytes(image)); sp = stack+60000
                    u.mem_write(sp, struct.pack('<I', end)); u.reg_write(UC_X86_REG_ESP, sp); u.reg_write(UC_X86_REG_EAX, config)
                    u.emu_start(0x10013e00, end, count=10000)
                    if 'error' in state: outcome = state['error']
                    else:
                        assert u.reg_read(UC_X86_REG_EIP) == end
                        value = u.reg_read(UC_X86_REG_EAX)
                        outcome = f'R:{value}'
                    rows.append(f'{failure} {response} {image.hex()}')
                    expected.append(outcome + ''.join(' '+v for v in state['trace']))
    actual = subprocess.check_output([str(a.executable), '--batch'], input='\n'.join(rows)+'\n', text=True).splitlines()
    assert len(actual) == len(expected)
    for i, (got, want) in enumerate(zip(actual, expected)):
        assert got == want, f'case {i}\nnative: {got}\noriginal: {want}'
    a.report.parent.mkdir(parents=True, exist_ok=True)
    a.report.write_text(json.dumps({'status':'passed', 'cases':len(rows), 'dll_sha256':digest,
        'addresses':['0x10013E00','0x10013FA0','0x10014010'],
        'compared':'Ordered factory, registration, mount, mount-table calls and arguments; signed failures, skipped devices and returned handle',
        'limits':'Original filesystem backends, logging and fatal termination are intercepted. No real mount, game boot or network operation occurs. Native fatal branches are exceptions; malformed unterminated strings are additionally rejected.'}, indent=2)+'\n')
    print(f'PASS {len(rows)} original AVS filesystem boot instruction comparisons')
if __name__ == '__main__': main()
