"""Shared isolated original x86 score fixture, bound to one DLL SHA-256.

No game process, Windows APIs, or transport is invoked. A note virtual query is an
explicit boolean fixture; all score/life/gauge/rate functions execute original code.
"""
from pathlib import Path
import hashlib
import itertools
import json
import random
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'.tools'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP,
    UC_X86_REG_EFLAGS)

data = (ROOT.parent/'modules/reflecbeat.dll').read_bytes()
digest = hashlib.sha256(data).hexdigest()
assert digest == 'ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
pe = pefile.PE(data=data); base = pe.OPTIONAL_HEADER.ImageBase
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
u.mem_write(base, pe.get_memory_mapped_image())
scratch, stack, end = 0x7000000, 0x8000000, 0x9000000
for address in (scratch, stack, end): u.mem_map(address, 65536)
battle, player, chart, note, manager, vtable, remote = [scratch+i*0x400 for i in range(7)]
config = scratch+0x4000
def word(n): return struct.pack('<I', n & 0xffffffff)
def put(address, n): u.mem_write(address, word(n))
def call(va, args=(), **regs):
    sp = stack+60000
    u.mem_write(sp, word(end)+b''.join(word(v) for v in args))
    u.reg_write(UC_X86_REG_ESP, sp); u.reg_write(UC_X86_REG_EFLAGS, 2)
    for name, reg in [('eax', UC_X86_REG_EAX), ('ebx', UC_X86_REG_EBX),
                      ('ecx', UC_X86_REG_ECX), ('edx', UC_X86_REG_EDX),
                      ('esi', UC_X86_REG_ESI), ('edi', UC_X86_REG_EDI)]:
        u.reg_write(reg, regs.get(name, 0)&0xffffffff)
    u.emu_start(va, end, count=10000)
    assert u.reg_read(UC_X86_REG_EIP) == end, 'instruction limit reached'
    return struct.unpack('<i', word(u.reg_read(UC_X86_REG_EAX)))[0]
def float_bits(va):
    wrapper, output = end+0x100, end+0x200
    # call original; fstp dword [output]; ret. Converts the original ST0 to float.
    u.mem_write(wrapper, b'\xe8'+word(va-wrapper-5)+b'\xd9\x1d'+word(output)+b'\xc3')
    u.ctl_remove_cache(wrapper, wrapper+32)
    call(wrapper, eax=player)
    return struct.unpack('<I', u.mem_read(output,4))[0]
put(0x12479290, manager); put(manager+12, player); put(manager+16, player)
put(manager+20, battle); put(0x12477e84, remote); put(0x10a71058, config)
put(note, vtable); put(note+160, chart); put(vtable+180, end+0x300)

def load(raw):
    u.mem_write(battle, bytes(raw[:172])); u.mem_write(player, bytes(raw[172:]))
def result(): return bytes(u.mem_read(battle,172)+u.mem_read(player,132)).hex()
def i32(raw, offset, n): struct.pack_into('<i', raw, offset, n)
def f32(raw, offset, n): struct.pack_into('<f', raw, offset, n)
rng = random.Random(2019121100)
def fixture():
    r = bytearray(304)
    for o in [0,4,8,12]: i32(r,o,rng.randint(0,9999))
    for i in range(5):
        for o in (20,40): f32(r,o+4*i,rng.choice([0.,.25,1.,5.5,99.,1000.,4000.]))
        f32(r,60+4*i,rng.choice([100.,5000.,10000.]))
        i32(r,144+4*i,[12,7,-4,16,-200][i])
        i32(r,172+4*i,rng.randint(0,100))
    for i in range(4): f32(r,80+4*i,rng.choice([0.,.1,.5,1.,1.1,2.75]))
    i32(r,96,rng.choice([0,1,7,100]))
    for o in (100,104): i32(r,o,rng.choice([0,10,200,201,400,401,999]))
    for o in (108,112): i32(r,o,rng.randint(0,100))
    for o in (136,140): i32(r,o,rng.choice([0,1,100,4999,5000]))
    i32(r,164,rng.choice([0,100]));i32(r,168,rng.choice([0,1,2,3,16,17,18,19]))
    for o in (192,196,232,236,240,244,264,268,272): i32(r,o,rng.randint(0,500))
    r[276]=1
    for o in (280,292,296): f32(r,o,rng.choice([0.,.25,.55,.65,.75,1.]))
    return r
