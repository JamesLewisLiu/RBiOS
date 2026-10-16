"""Offline request-field capture and callback-status parity against the original DLL.

AVS property creation/string copy are mocked in emulated memory, not networked.
The original field-builder instructions and card-type/status branches execute.
"""
from pathlib import Path
import hashlib
import itertools
import json
import os
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.tools'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from kbinxml import KBinXML
from lxml import etree

data = (ROOT.parent / 'modules/reflecbeat.dll').read_bytes()
digest = hashlib.sha256(data).hexdigest()
assert digest == 'ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
exe = ROOT / '.build' / ('rbprotocoltests.exe' if sys.platform == 'win32' else 'rbprotocoltests')
exe.parent.mkdir(exist_ok=True)
zig = ROOT / '.tools/ziglang/zig.exe'
compiler = [str(zig), 'c++'] if zig.exists() else [shutil.which('clang++') or 'c++']
env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(ROOT / '.cache/zig-global'), ZIG_LOCAL_CACHE_DIR=str(ROOT / '.cache/zig-local'))
subprocess.run(compiler + ['-std=c++17', '-O1', '-Wall', '-Wextra', '-ffp-contract=off', '-Icore/include',
                           'core/src/Protocol.cpp', 'core/src/Property.cpp', 'tests/ProtocolTests.cpp',
                           '-o', str(exe)], cwd=ROOT, env=env, check=True)
subprocess.run([str(exe)], check=True)
pe = pefile.PE(data=data)
base = pe.OPTIONAL_HEADER.ImageBase
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
u.mem_write(base, pe.get_memory_mapped_image())
mem, stack, stub, end = 0x07000000, 0x08000000, 0x06000000, 0x09000000
for address in (mem, stack, stub, end): u.mem_map(address, 65536)

def cstring(address):
    b = bytearray()
    while len(b) < 1024:
        c = bytes(u.mem_read(address + len(b), 1))
        if c == b'\0': return bytes(b)
        b.extend(c)
    raise AssertionError('unterminated emulated string')

captured = []
def hook(machine, address, size, _):
    if address not in (stub, stub + 16, stub + 32): return
    sp = u.reg_read(UC_X86_REG_ESP)
    ret = struct.unpack('<I', u.mem_read(sp, 4))[0]
    if address == stub:
        _, _, type_id, name, value = struct.unpack('<5I', u.mem_read(sp + 4, 20))
        val = cstring(value).decode('ascii') if type_id == 11 else str(struct.unpack('<i', struct.pack('<I', value))[0])
        captured.append((cstring(name).decode('ascii'), type_id, val))
        result = mem + 50000
    elif address == stub + 32:
        dest, source, limit = struct.unpack('<3I', u.mem_read(sp + 4, 12))
        b = cstring(source)
        assert len(b) < limit, 'only in-capacity original strings are covered'
        u.mem_write(dest, b + b'\0'); result = dest
    else: result = 1
    u.reg_write(UC_X86_REG_EAX, result)
    u.reg_write(UC_X86_REG_ESP, sp + 4)
    u.reg_write(UC_X86_REG_EIP, ret)

u.hook_add(UC_HOOK_CODE, hook)
for iat, target in ((0x10189744, stub), (0x101896c4, stub + 16), (0x10189720, stub + 32)):
    u.mem_write(iat, struct.pack('<I', target))

def call(address, eax=0, ecx=0, esi=0, args=()):
    sp = stack + 60000
    u.mem_write(sp, struct.pack('<' + 'I' * (1 + len(args)), end, *[x & 0xffffffff for x in args]))
    for register, value in ((UC_X86_REG_ESP, sp), (UC_X86_REG_EAX, eax), (UC_X86_REG_ECX, ecx), (UC_X86_REG_ESI, esi)):
        u.reg_write(register, value & 0xffffffff)
    u.emu_start(address, end, count=10000)
    assert u.reg_read(UC_X86_REG_EIP) == end
    return u.reg_read(UC_X86_REG_EAX)

actor = mem
rid, lid, card = mem + 4096, mem + 8192, mem + 12288
for address, value in ((rid, b'0123456789ABCDEF'), (lid, b'ABCD1234'), (card, b'FEDCBA9876543210')):
    u.mem_write(address, value + b'\0')
field_cases = []
for kind in (-2147483648, -1, 0, 1, 2, 3, 32767, 2147483647):
    u.mem_write(actor, bytes(1024))
    call(0x1009cfe0, eax=rid, esi=actor + 548, args=(lid, -32768, card, kind))
    captured.clear()
    assert call(0x1009cee0, ecx=actor, args=(mem + 40000,)) & 255 == 1
    tree = etree.Element('synthetic')
    for name, type_id, value in captured:
        node = etree.SubElement(tree, name, __type='str' if type_id == 11 else 's16'); node.text = value
    expected = KBinXML(tree).to_binary()
    actual = bytes.fromhex(subprocess.check_output([str(exe), '--read-fields', str(kind)]).decode().strip())
    assert actual == expected, (kind, captured)
    field_cases.append({'input_card_type': kind, 'fields': list(captured)})

captured.clear()
call(0x1009c490, eax=rid, ecx=actor + 548)
assert call(0x1009c430, ecx=actor, args=(mem + 40000,)) & 255 == 1
assert captured == [('rid', 11, '0123456789ABCDEF')]
score_fields = list(captured)
inputs, expected = [], []
for first, code, third, pending in itertools.product((0, 1), (-2147483648, *range(-1, 141), 2147483647), (0, -1), (False, True)):
    u.mem_write(actor, struct.pack('<5I', first, 0, code & 0xffffffff, third & 0xffffffff, 0) + bytes([pending]))
    expected.append(str(call(0x1009f1b0, ecx=actor)))
    inputs.append(f'{first} {code} {third} {int(pending)}')
actual = subprocess.check_output([str(exe), '--status'], input=('\n'.join(inputs) + '\n').encode()).decode().splitlines()
assert actual == expected
report = {'dll_sha256': digest, 'player_read_cases': len(field_cases), 'player_read_score_schema_cases': 1,
          'callback_status_cases': len(inputs), 'request_fields': field_cases,
          'score_fields': score_fields,
          'original_functions': {'read_prepare': '0x1009CFE0', 'read_fields': '0x1009CEE0',
                                 'read_score_prepare': '0x1009C490', 'read_score_fields': '0x1009C430',
                                 'callback_status': '0x1009F1B0'},
          'method': 'original x86 field builders with mocked AVS allocation/string-copy; original callback status function without imports',
          'limitations': ['no EA3 envelope, compression, HTTP headers, services/cardmng schema or server compatibility',
                         'only in-capacity ASCII sample strings; original AVS truncation not recovered']}
(ROOT / '.build/protocol-parity.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(f'PASS {len(field_cases)} player read field captures, 1 score schema, {len(inputs)} original callback status cases (no transport)')
