"""Verify the recovered bounded path grammar against original AVS x86 code."""
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
    for name in ('data', 'modules', 'prop'):
        if a.report.resolve().is_relative_to((a.game_root / name).resolve()): raise ValueError('Report overlaps original assets')
    sys.path.insert(0, str(a.dependencies.resolve()))
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP
    raw = (a.game_root / 'modules/libavs-win32.dll').read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    assert digest == 'd062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe = pefile.PE(data=raw); base = pe.OPTIONAL_HEADER.ImageBase
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    u.mem_write(base, pe.get_memory_mapped_image()); u.mem_map(0x7000000, 0x30000)
    events = []; errors = []
    def hook(_u, addr, size, data):
        if addr not in (0x7010000, 0x7010010, 0x1002fc60): return
        sp = u.reg_read(UC_X86_REG_ESP)
        args = struct.unpack('<10I', u.mem_read(sp, 40))
        if addr == 0x1002fc60:
            errors.append(args[2]); value = args[2]
        else:
            pointer, length = struct.unpack('<2I', u.mem_read(args[4], 8))
            name = bytes(u.mem_read(pointer, length)).decode('ascii')
            if addr == 0x7010000:
                selector = struct.unpack('<i', struct.pack('<I', args[5]))[0]
                events.append(f'N:{name}:{selector}:{args[7]}')
            else: events.append(f'A:{name}')
            value = 0x1000 + len(events)
        u.reg_write(UC_X86_REG_EAX, value)
        u.reg_write(UC_X86_REG_ESP, sp + 4); u.reg_write(UC_X86_REG_EIP, args[0])
    u.hook_add(UC_HOOK_CODE, hook)
    paths = ['', '/', '///', 'config', '/config//fs/root/', 'id@', 'id@item#-1', '__type@item',
             'a@b@c', 'item#', 'item#-', 'item#08', 'x/item#abc', 'item#0', 'item#-1',
             'item#0x10', 'item#01', 'item#+2', 'item# 2', 'item#-0', 'a/id@item']
    rng = random.Random(0x575d0)
    for _ in range(512):
        parts = [rng.choice(['fs', 'thread', 'item', '_test', 'A:1']) + rng.choice(['', '#0', '#1', '#-1', '#-2', '#017', '#0xA']) for _ in range(rng.randint(1, 5))]
        path = rng.choice(['', '/', '//']) + rng.choice(['/', '//']).join(parts) + rng.choice(['', '/', '///'])
        if rng.randrange(3) == 0: path = rng.choice(['id@', 'value@', '__hidden@']) + path
        paths.append(path)
    expected = []
    for path in paths:
        events.clear(); errors.clear(); u.mem_write(0x7000000, path.encode() + b'\0')
        sp = 0x702f000
        u.mem_write(sp, struct.pack('<8I', 0x7010020, 0x7001000, 0xffffffff, 0x1000, 0x7000000, 0x7010000, 0x7010010, 0))
        u.reg_write(UC_X86_REG_ESP, sp); u.emu_start(0x100575d0, 0x7010020, count=100000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x7010020
        status = 0 if not errors else {0x80092040: 1, 0x80092042: 2}[errors[-1]]
        expected.append(str(status) + ''.join(' ' + event for event in events))
    got = subprocess.check_output([str(a.executable), '--batch'], input='\n'.join(paths) + '\n', text=True).splitlines()
    assert len(got) == len(expected)
    for path, actual, wanted in zip(paths, got, expected): assert actual == wanted, (path, actual, wanted)
    # Synthetic original node arena: real node_refer, parser, child/attribute
    # searches and name comparator execute, with no lookup replacements.
    arena = 0x7002000; common = arena + 48 * 8
    names = ['config', 'item', 'other', 'item', 'leaf', 'id', 'id']
    def index(identifier): return 8 - identifier if identifier is not None else 0
    for identifier, name in enumerate(names):
        node = arena + identifier * 48; string = 0x7004000 + identifier * 32
        u.mem_write(node, bytes(48)); u.mem_write(string, name.encode())
        u.mem_write(node, struct.pack('<II', string, len(name)))
        u.mem_write(node + 24, b'\xfe')
        u.mem_write(node + 32, struct.pack('<H', index(identifier)))
    def links(identifier, previous=None, following=None, child=None, attribute=None):
        u.mem_write(arena + identifier * 48 + 34, struct.pack('<5H', index(previous), index(following), 0, index(child), index(attribute)))
    links(0, child=1); links(1, following=2, attribute=5); links(2, previous=1, following=3)
    links(3, previous=2, child=4, attribute=6); links(4); links(5); links(6)
    u.mem_write(common + 28, struct.pack('<I', 24))
    references = ['', '/', 'item', '/item', 'item#0', 'item#1', 'item#-1', 'item#-2', 'item#2',
                  'item#-3', 'other', 'other#-1', 'item#1/leaf', '/item#-1//leaf/', 'id@item',
                  'id@item#1', 'id@item#-2', 'id@other', 'missing', 'config']
    resolved = []
    for path in references:
        errors.clear(); u.mem_write(0x7000000, path.encode() + b'\0'); sp = 0x702f000
        u.mem_write(sp, struct.pack('<4I', 0x7010020, 0, arena, 0x7000000))
        u.reg_write(UC_X86_REG_ESP, sp); u.emu_start(0x1000cc40, 0x7010020, count=100000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x7010020
        answer = u.reg_read(UC_X86_REG_EAX)
        if not answer: resolved.append('missing')
        else:
            assert (answer - arena) % 48 == 0
            identifier = (answer - arena) // 48
            resolved.append(f'N:{identifier}' if identifier < 5 else f'A:{97 + identifier - 5}')
    native = subprocess.check_output([str(a.executable), '--resolve'], input='\n'.join(references) + '\n', text=True).splitlines()
    assert native == resolved, (native, resolved)
    a.report.parent.mkdir(parents=True, exist_ok=True)
    a.report.write_text(json.dumps({'status': 'passed', 'cases': len(paths), 'lookup_cases': len(references), 'dll_sha256': digest,
        'address': '0x100575D0', 'compared': 'Ordered name/selector/attribute-context callbacks, attribute name and syntax/reserved errors',
        'limits': 'Parser trace cases use recording lookup callbacks. Lookup cases execute original property_search, parser, name matching and forward/reverse child/attribute traversal on a synthetic original node arena; only error reporting is replaced. Property allocation, XML parsing and packed node-name mode remain outside scope.'}, indent=2) + '\n')
    print(f'PASS {len(paths)} original AVS path-parser and {len(references)} full lookup comparisons')
if __name__ == '__main__': main()
