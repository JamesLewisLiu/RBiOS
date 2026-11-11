"""Compare native XML token semantics with complete original AVS XML loading."""
import argparse, hashlib, json, struct, subprocess, sys
from pathlib import Path
import xml.etree.ElementTree as ET

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('game-root', 'dependencies', 'executable', 'report'): p.add_argument('--' + key, type=Path, required=True)
    a = p.parse_args()
    if a.report.exists(): raise ValueError('Report already exists')
    for name in ('data', 'modules', 'prop'):
        if a.report.resolve().is_relative_to((a.game_root / name).resolve()): raise ValueError('Report overlaps original assets')
    sys.path.insert(0, str(a.dependencies.resolve()))
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP
    dll = (a.game_root / 'modules/libavs-win32.dll').read_bytes(); digest = hashlib.sha256(dll).hexdigest()
    assert digest == 'd062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe = pefile.PE(data=dll); base = pe.OPTIONAL_HEADER.ImageBase
    u = Uc(UC_ARCH_X86, UC_MODE_32); u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    u.mem_write(base, pe.get_memory_mapped_image()); u.mem_map(0x7000000, 0x300000)
    raw = b''; position = 0
    def reader(_u, address, size, data):
        nonlocal position
        if address != 0x7100000: return
        sp = u.reg_read(UC_X86_REG_ESP); ret, context, destination, count = struct.unpack('<4I', u.mem_read(sp, 16))
        chunk = raw[position:position + count]; position += len(chunk)
        if chunk: u.mem_write(destination, chunk)
        u.reg_write(UC_X86_REG_EAX, len(chunk)); u.reg_write(UC_X86_REG_ESP, sp + 4); u.reg_write(UC_X86_REG_EIP, ret)
    u.hook_add(UC_HOOK_CODE, reader)
    def call(address, *args):
        sp = 0x72ff000; u.mem_write(sp, struct.pack('<' + 'I' * (len(args) + 1), 0x7100010, *args))
        u.reg_write(UC_X86_REG_ESP, sp); u.emu_start(address, 0x7100010, count=3000000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x7100010
        return u.reg_read(UC_X86_REG_EAX)
    def node_info(node):
        data = bytes(u.mem_read(node, 48)); pointer, length = struct.unpack_from('<II', data)
        name = bytes(u.mem_read(pointer, length)); common = node + 48 * (int.from_bytes(data[32:34], 'little') + (int.from_bytes(data[12:14], 'little') << 16))
        def link(low, high):
            index = int.from_bytes(data[low:low+2], 'little') + (int.from_bytes(data[high:high+2], 'little') << 16)
            return common - 48 * index if index else 0
        length = int.from_bytes(data[44:47], 'little')
        pointer = node + 28 if length <= 4 else (node - int.from_bytes(data[28:32], 'little')) & 0xffffffff
        value = bytes(u.mem_read(pointer, length)) if length else b''
        kind = data[47] & 63
        if 2 <= kind <= 9:
            width = 1 << ((kind - 2) // 2)
            value = b''.join(value[i:i+width][::-1] for i in range(0, len(value), width))
        return name, kind, bool(data[47] & 64), value, link(40, 20), link(36, 16), link(42, 22)
    def dump(node):
        name, kind, array, value, child, following, attr = node_info(node)
        lines = [f'N {name.hex()} {kind} {int(array)} {value.hex() or "-"}']
        attrs = []
        while attr:
            key, _, _, value, _, attr, _ = node_info(attr); attrs.append((key, value))
        lines += [f'A {key.hex()} {value.hex() or "-"}' for key, value in sorted(attrs)]
        while child:
            lines += dump(child); child = node_info(child)[5]
        return lines + ['X']
    original = (a.game_root / 'prop/avs-config.xml').read_bytes()
    documents = [original]
    for kind in ('', 'str', 'u8', 'u16', 'u32', 'bool'):
        for value in ('', '0', '1', '08', '1A', ' 12 ', '1 2', '4294967296'):
            attr = f' __type="{kind}"' if kind else ''
            documents.append(f'<config><v{attr}>{value}</v></config>'.encode())
    documents += [
        '<config><v __type="str" id="\u65e5\u672c\u8a9e">\u65e5\u672c\u8a9e &amp; テスト</v></config>'.encode('cp932'),
        b'<config><v __type="u16" __count="3">1 2</v></config>',
        b'<config><v __type="u16" __count="2">1 2 3</v></config>',
        b'<config><v __type="u16" __count="0"/></config>',
        b'<config><v id="a"/><v id="b"/></config>',
        b'<config><v __unknown="1"/></config>',
        b'<config><v __type="str" __count="2">a</v></config>']
    successful = 0
    for index, raw in enumerate(documents):
        if not raw.startswith(b'<?xml'): raw = b'<?xml version="1.0" encoding="Shift-JIS"?>' + raw
        position = 0; prop = call(0x1000b080, 0x1017, 0x7000000, 0x100000)
        status = call(0x1000b180, prop, 0, 0x7100000, 0)
        if status & 0x80000000: expected = ['ERROR']
        else:
            u.mem_write(0x7101000, b'config\0'); root = call(0x1000cc40, prop, 0, 0x7101000)
            assert root; expected = dump(root); successful += 1
        tokens = []
        def encoded(text): return text.encode('cp932').hex() or '-'
        def emit(node):
            tokens.append(f'B {encoded(node.tag)} {len(node.attrib)}' + ''.join(f' {encoded(k)} {encoded(v)}' for k, v in node.attrib.items()))
            # Split content to check platform callback chunk independence.
            if node.text:
                for part in (node.text[:1], node.text[1:]):
                    if part: tokens.append('T ' + encoded(part))
            for child in node:
                emit(child)
                if child.tail: tokens.append('T ' + encoded(child.tail))
            tokens.append('E ' + encoded(node.tag))
        emit(ET.fromstring(raw.decode('cp932')))
        actual = subprocess.check_output([str(a.executable), '--tokens'], input='\n'.join(tokens) + '\n', text=True).splitlines()
        assert actual == expected, (index, raw, actual, expected)
    a.report.parent.mkdir(parents=True, exist_ok=True)
    a.report.write_text(json.dumps({'status': 'passed', 'documents': len(documents), 'successful_trees': successful,
        'dll_sha256': digest, 'original_config_sha256': hashlib.sha256(original).hexdigest(),
        'compared': 'Complete node tree, type/array flags, big-endian numeric bytes, original CP932 string/attribute bytes and success/error outcome',
        'limits': 'Original property_create/insert_read/search/XML parsing/typed conversion execute unchanged; only the input-reader callback supplies bytes. Native XML tokenization is platform-owned (ElementTree fixtures here). __size, mixed content and other scalar types are explicitly unsupported; no writer is implemented.'}, indent=2) + '\n')
    print(f'PASS {len(documents)} full original XML comparisons ({successful} decoded trees)')
if __name__ == '__main__': main()
