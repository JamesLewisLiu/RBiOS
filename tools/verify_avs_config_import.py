"""Compare native AVS boot import with original psmap instructions, offline."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--dependencies', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if args.report.exists():
        raise ValueError('Report already exists')
    for name in ('data', 'modules', 'prop'):
        if args.report.resolve().is_relative_to((args.game_root / name).resolve()):
            raise ValueError('Report must be outside original assets')
    sys.path.insert(0, str(args.dependencies.resolve()))
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP

    raw = (args.game_root / 'modules/libavs-win32.dll').read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    assert digest == 'd062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe = pefile.PE(data=raw)
    base = pe.OPTIONAL_HEADER.ImageBase
    image = pe.get_memory_mapped_image()

    def string(address):
        start = address - base
        return image[start:image.index(b'\0', start)].decode('ascii')

    fields = []
    for index in range(49):
        handle, flags, offset, size, path, default = struct.unpack_from('<BBHIII', image, 0x76c20 + index * 16)
        if handle == 255:
            break
        assert flags == 1
        fields.append((string(path), image[0x67040 + handle], size))
    assert len(fields) == 48 and handle == 255
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    u.mem_write(base, image)
    config, stack, end, arena = 0x7000000, 0x8000000, 0x9000000, 0xa000000
    for address in (config, stack, end, arena):
        u.mem_map(address, 65536)
    values = {}
    nodes = {}
    calls = []

    def return_value(value):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', u.mem_read(sp, 4))[0]
        u.reg_write(UC_X86_REG_EAX, value & 0xffffffff)
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, ret)

    def hook(_u, address, _size, _data):
        sp = u.reg_read(UC_X86_REG_ESP)
        if address == 0x1000cc40:  # property_search: test property tree boundary
            _, document, parent, path = struct.unpack('<4I', u.mem_read(sp, 16))
            assert document == 0 and parent == 0x1234
            name = string(path)
            calls.append(name)
            return_value(nodes.get(name, 0))

    u.hook_add(UC_HOOK_CODE, hook)
    rng = random.Random(0x76c20)
    cases = [{}]
    for path, kind, size in fields:
        for variant in range(4):
            if kind == 11:
                length = (0, 1, size // 2, size - 1)[variant]
                data = bytes(rng.randint(33, 126) for _ in range(length)) + b'\0'
            else:
                value = (0, 1, (1 << (8 * size)) - 1, rng.getrandbits(8 * size))[variant]
                data = value.to_bytes(size, 'big')
            cases.append({path: (kind, data)})
        cases.append({path: (kind | 64, data)})
        if kind == 11:
            for data in (b'', b'x' * size, b'a\0b\0'):
                cases.append({path: (kind, data)})
    # The XML is read as fixture input only. This does not claim AVS XML parser recovery.
    original_config = args.game_root / 'prop/avs-config.xml'
    root = ET.fromstring(original_config.read_bytes().decode('shift_jis'))
    xml_values = {}
    types = {'u16': 5, 's32': 6, 'u32': 7, 'u64': 9, 'str': 11, 'bool': 52}
    for path, kind, size in fields:
        node = root.find(path)
        if node is not None:
            assert types[node.attrib['__type']] == kind
            data = ((node.text or '').encode('shift_jis') + b'\0' if kind == 11 else
                    (int(node.text) & ((1 << (8 * size)) - 1)).to_bytes(size, 'big'))
            xml_values[path] = (kind, data)
    cases.append(xml_values)
    expected = []
    rows = []
    for values in cases:
        nodes = {name: arena + index * 64 for index, name in enumerate(values)}
        for index, (name, (kind, data)) in enumerate(values.items()):
            node = nodes[name]
            payload = data if (kind & 63) == 11 else data[::-1]
            u.mem_write(node, bytes(48))
            u.mem_write(node + 44, len(payload).to_bytes(3, 'little') + bytes([kind]))
            if 0 < len(payload) <= 4:
                u.mem_write(node + 28, payload)
            elif payload:
                storage = arena + 32768 + index * 512
                u.mem_write(storage, payload)
                u.mem_write(node + 28, struct.pack('<I', (node - storage) & 0xffffffff))
        calls.clear()
        u.mem_write(config, bytes(2528))
        sp = stack + 60000
        # Defaults-only case passes no document/node: no hooks are reached.
        u.mem_write(sp, struct.pack('<5I', end, 0, 0x1234 if values else 0, config, 0x10076c20))
        u.reg_write(UC_X86_REG_ESP, sp)
        u.emu_start(0x10006c80, end, count=100000)
        assert u.reg_read(UC_X86_REG_EIP) == end and u.reg_read(UC_X86_REG_EAX) == 1
        assert calls == ([p for p, _, _ in fields] if values else [])
        expected.append(bytes(u.mem_read(config, 2528)).hex())
        rows.append(str(len(values)) + ''.join(f' {name} {kind} {data.hex() or "-"}' for name, (kind, data) in values.items()))
    actual = subprocess.check_output([str(args.executable), '--batch'], input='\n'.join(rows) + '\n', text=True).splitlines()
    assert len(actual) == len(expected)
    for index, (got, want) in enumerate(zip(actual, expected)):
        assert got == want, f'case {index} mismatch'
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({
        'status': 'passed', 'cases': len(cases), 'dll_sha256': digest,
        'config_sha256': hashlib.sha256(original_config.read_bytes()).hexdigest(),
        'addresses': ['0x10006C80', '0x10006C8C', '0x10006E20', '0x10007160', '0x1000D490', '0x10034000', '0x100340CC', '0x10076C20'],
        'compared': 'All 2528 image bytes; lookup order; 48 defaults; typed overrides, array flags, raw string boundaries; original config fixture',
        'limits': 'Defaults run original instructions without stubs. Supplied values replace only property_search with a lookup callback to synthetic nodes; original node_read and byte-copy execute. XML parsing, property allocation/path lookup, full boot, filesystem and networking are not executed.'
    }, indent=2) + '\n')
    print(f'PASS {len(cases)} original AVS boot config image comparisons')


if __name__ == '__main__':
    main()
