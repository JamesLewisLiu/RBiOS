"""Original CLongNoteGraphic / CSwitchLongGraphic arithmetic and draw commands.

Virtual render setters are capture-only adapters. Math and clipping execute the
shipped x86 instructions. No game process, Windows API, file conversion or network.
"""
from pathlib import Path
import hashlib
import json
import random
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.tools'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *

data = (ROOT.parent / 'modules/reflecbeat.dll').read_bytes()
digest = hashlib.sha256(data).hexdigest()
assert digest == 'ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
pe = pefile.PE(data=data)
u = Uc(UC_ARCH_X86, UC_MODE_32)
base = pe.OPTIONAL_HEADER.ImageBase
u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
u.mem_write(base, pe.get_memory_mapped_image())
scratch, stack, stop = 0x7000000, 0x8000000, 0x9000000
for a in (scratch, stack, stop):
    u.mem_map(a, 0x100000)

def wi(a, v): u.mem_write(a, struct.pack('<I', v & 0xffffffff))
def wf(a, v): u.mem_write(a, struct.pack('<f', v))
def ri(a): return struct.unpack('<i', u.mem_read(a, 4))[0]
def ru(a): return struct.unpack('<I', u.mem_read(a, 4))[0]
def rf(a): return struct.unpack('<f', u.mem_read(a, 4))[0]
def bits(v): return struct.unpack('<I', struct.pack('<f', v))[0]
def text(a):
    b = bytearray()
    while u.mem_read(a, 1)[0]:
        b.extend(u.mem_read(a, 1)); a += 1
    return b.decode()

properties = {}
overlays = []
slots = {16: 4, 20: 0, 28: 8, 32: 4, 36: 8, 64: 16, 80: 16,
         88: 8, 92: 4, 104: 4, 148: 0}
vtable = scratch + 0x80000
for slot, size in slots.items():
    address = stop + 0x100 + slot * 4
    wi(vtable + slot, address)
    u.mem_write(address, b'\xc2' + struct.pack('<H', size))

def ret(size):
    sp = u.reg_read(UC_X86_REG_ESP)
    u.reg_write(UC_X86_REG_EIP, ru(sp))
    u.reg_write(UC_X86_REG_ESP, sp + 4 + size)

def hook(_, address, __, ___):
    sp = u.reg_read(UC_X86_REG_ESP)
    obj = u.reg_read(UC_X86_REG_ECX)
    if stop + 0x100 <= address <= stop + 0x100 + 148 * 4:
        slot = (address - stop - 0x100) // 4
        p = properties.setdefault(obj, dict(x=0, y=0, angle=0., sx=1., sy=1., alpha=1., visible=True))
        if slot == 16: p['visible'] = bool(ru(sp + 4))
        elif slot == 28: p.update(x=ri(sp + 4), y=ri(sp + 8))
        elif slot == 32: p['angle'] = rf(sp + 4)
        elif slot == 64: p['alpha'] = rf(sp + 4)
        elif slot == 88: p.update(sx=rf(sp + 4), sy=rf(sp + 8))
        elif slot == 92: p.update(sx=rf(sp + 4), sy=rf(sp + 4))
        elif slot == 104: p['layer'] = ri(sp + 4)
        elif slot == 148: u.reg_write(UC_X86_REG_EAX, 1) # effect already playing
    elif address == 0x1002afe0: # only bind sprite resource, return synthetic sprite
        properties.setdefault(obj, {})['name'] = text(ru(sp + 4))
        ret(4)
    elif address == 0x10164c30: # movie-pool binding; allocator is outside geometry
        ret(0)
    elif address == 0x101576f0: # raw original effect queue record
        p = ru(sp + 4)
        overlays.append((u.reg_read(UC_X86_REG_EAX), ri(p), ri(p + 4), ru(p + 12), ru(p + 20)))
        ret(4)

u.hook_add(UC_HOOK_CODE, hook)

def call(va, args=(), **regs):
    sp = stack + 0xf0000
    wi(sp, stop)
    for i, value in enumerate(args):
        if isinstance(value, float): wf(sp + 4 + 4 * i, value)
        else: wi(sp + 4 + 4 * i, value)
    for name, reg in [('eax', UC_X86_REG_EAX), ('ebx', UC_X86_REG_EBX),
                      ('ecx', UC_X86_REG_ECX), ('edx', UC_X86_REG_EDX),
                      ('esi', UC_X86_REG_ESI), ('edi', UC_X86_REG_EDI)]:
        u.reg_write(reg, regs.get(name, 0) & 0xffffffff)
    u.reg_write(UC_X86_REG_ESP, sp)
    u.reg_write(UC_X86_REG_FPCW, 0x37f)
    try:
        u.emu_start(va, stop, count=100000)
    except Exception:
        print('original execution error', hex(va), hex(u.reg_read(UC_X86_REG_EIP)))
        raise
    assert u.reg_read(UC_X86_REG_EIP) == stop, hex(va)

def sprite(a, name='', layer=40):
    wi(a, vtable)
    properties[a] = dict(name=name, layer=layer, x=0, y=0, angle=0., sx=1., sy=1., alpha=1., visible=True)

def actor(a, inner):
    wi(a, vtable)
    wi(a + 116, a + 128 if inner else 0)
    wi(a + 128, inner)
    if inner: sprite(inner)

def output(records, fx=()):
    values = [str(len(records))]
    for name, p, layer, anchor, blend in records:
        values += [name, str(p['x']), str(p['y']), str(layer), str(bits(p['angle'])),
                   str(bits(p['sx'])), str(bits(p['sy'])), str(bits(p['alpha'])),
                   str(bits(anchor)), str(blend)]
    values += [str(len(fx))]
    for effect in fx: values += list(map(str, effect))
    return ' '.join(values)

commands, expected = [], []
rng = random.Random(2019121100)
counts = {'long': 0, 'switch': 0}
stage = scratch + 0x70000
wi(0x1247929c, stage)
wi(0x124792a0, scratch + 0x71000)
chart = scratch + 0x72000

for case in range(180):
    properties.clear()
    g = scratch
    u.mem_write(g, bytes(4096)); wi(g, vtable)
    headactor, tailactor = scratch + 0x1000, scratch + 0x2000
    actor(headactor, 0); actor(tailactor, 0)
    wi(g + 112, headactor); wi(g + 116, tailactor)
    sprites = [scratch + 0x3000 + 0x1000 * i for i in range(3)]
    for i, a in enumerate(sprites): sprite(a); wi(g + 148 + i * 4, a)
    color, top, side, flags, mode = case % 2, (case // 2) % 2, (case // 4) % 2, (case // 8) % 2 * 8, case % 3 == 0
    wi(chart + 280, color); wi(chart + 284, top); wi(chart + 328, side)
    call(0x1015c1a0, ecx=g, args=(chart, flags))
    u.mem_write(g + 20, b'\x01')
    x, y, tx, ty = [rng.randint(20, 1200) for _ in range(4)]
    scale, tail_scale, alpha = rng.choice([.5, .75, 1., 1.25]), rng.choice([.5, 1., 1.5]), rng.choice([.25, .5, 1.])
    vx, vy = rng.choice([-16., -3., 0., 3., 16.]), rng.choice([-12., -4., 4., 12.])
    wf(stage + 24, scale); wf(g + 136, alpha); wf(g + 144, tail_scale)
    if mode:
        call(0x1015c400, eax=y, ecx=x, esi=g, args=(vx, vy))
    else:
        call(0x1015c2d0, eax=ty, edx=g, ecx=tx, args=(x, y))
    call(0x1015bf90, ecx=g)
    record = [(properties[a]['name'], properties[a], (62, 39 if top else 40, 61)[i], 0. if i == 0 else .5, 8 if i != 1 else 0)
              for i, a in enumerate(sprites)]
    commands.append(f'L {color} {top} {side} {flags} {int(mode)} {x} {y} {tx} {ty} {scale} {tail_scale} {alpha} {vx} {vy}')
    expected.append(output(record)); counts['long'] += 1

for case in range(240):
    properties.clear(); overlays.clear()
    g = scratch
    u.mem_write(g, bytes(4096)); wi(g, vtable)
    active, held, color = case % 17 != 0, case % 2, (case // 2) % 2
    count = case % 16 + 1
    head = (rng.randint(100, 668), rng.randint(500, 1180))
    trail = (head[0] + rng.randint(-50, 50), head[1] - rng.randint(-30, 450))
    vx, vy = rng.randint(-8, 8) / 2., rng.choice([2., 4., 8., 16.])
    ticks, secondary, clip = rng.randint(-2, 100), rng.randint(0, 80), rng.randint(200, 1250)
    scale, alpha = rng.choice([.5, .75, 1., 1.25]), rng.choice([.25, .5, 1.])
    u.mem_write(g + 20, bytes([active])); wi(g + 316, head[0]); wi(g + 320, head[1])
    wi(g + 324, trail[0]); wi(g + 328, trail[1]); wf(g + 332, vx); wf(g + 336, vy)
    wi(g + 340, ticks); wi(g + 344, secondary); wf(g + 348, alpha); wf(g + 352, scale)
    wi(g + 616, count); wi(g + 620, clip); wi(g + 624, color)
    bgactor, tail = scratch + 0x1000, scratch + 0x2000
    bginner = scratch + 0x3000
    actor(bgactor, bginner); sprite(tail)
    wi(g + 304, bgactor); wi(g + 308, tail)
    handles, parts = [], []
    for i in range(count):
        duration = rng.randint(1, 30)
        collection, special, secondary_special = bool(case % 7 == 0), bool(case % 5 == 0), bool(case % 3 == 0)
        parts.append((duration, int(collection), int(special), int(secondary_special)))
        wi(g + 360 + 16 * i, duration)
        u.mem_write(g + 372 + 16 * i, bytes([collection, special, secondary_special]))
        row = []
        for j in range(3):
            a = scratch + 0x4000 + (i * 3 + j) * 0x200
            inner = a + 0x100
            actor(a, inner); wi(g + (112, 176, 240)[j] + i * 4, a); row.append(inner)
        handles.append(row)
    call(0x10157250, ecx=g)
    call(0x10156d40, ecx=g)
    record = []
    if active and ticks > 0:
        for i, row in enumerate(handles):
            body, cap, collection = [properties[a] for a in row]
            if body['visible']:
                record.append(('swich_long_body_off' if i % 2 else 'swich_long_body_on', body, 62, 1., 0))
                if i + 1 == count and i % 2 == 0:
                    record.append(('long_tail_switch', properties[tail], 40, .5, 0))
                    record.append(('long_tail_bg_switch', properties[bginner], 40, .5, 0))
                else:
                    record.append(('swich_on' if i % 2 else 'swich_off', cap, 40, .5, 0))
                if parts[i][1]: record.append(('eff_justcollection', collection, 38, .5, 0))
    commands.append('S ' + ' '.join(map(str, [int(active), held, color, *head,
                           ri(g + 324), ri(g + 328), vx, vy, ticks, secondary, clip, scale, alpha, count,
                           *(v for part in parts for v in part)])))
    expected.append(output(record, overlays)); counts['switch'] += 1

exe = ROOT / '.build' / ('rblonggraphic.exe' if sys.platform == 'win32' else 'rblonggraphic')
actual = subprocess.check_output([str(exe), '--batch'], input=('\n'.join(commands) + '\n').encode()).decode().splitlines()
assert len(actual) == len(expected)
for command, wanted, result in zip(commands, expected, actual):
    if wanted != result:
        raise AssertionError((command, wanted, result))
report = dict(dll_sha256=digest, cases=sum(counts.values()), coverage=counts,
              method='bounded original x86 geometry/draw execution; capture-only virtual renderer',
              limitations=['original AFP movie playback is a separate runtime',
                           'positive bounded switch durations; original stage coordinates',
                           'movie allocation and resource binding use capture-only adapters',
                           'labels/alignments/layers also cross-checked against original initialization instructions'])
(ROOT / '.build/long-graphic-parity.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report))
