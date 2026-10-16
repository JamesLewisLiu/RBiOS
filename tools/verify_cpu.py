"""Bounded offline CPU parity: original x86 functions, no game process/network.

Initialization is compared in stages: original distribution, original shuffle
helper and original offset pass; the original CRT vector allocator is bypassed.
"""
from pathlib import Path
import collections
import hashlib
import itertools
import json
import os
import random
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.tools'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP

data = (ROOT.parent / 'modules/reflecbeat.dll').read_bytes()
digest = hashlib.sha256(data).hexdigest()
assert digest == 'ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
exe = ROOT / '.build' / ('rbcputests.exe' if sys.platform == 'win32' else 'rbcputests')
exe.parent.mkdir(exist_ok=True)
zig = ROOT / '.tools/ziglang/zig.exe'
compiler = [str(zig), 'c++'] if zig.exists() else [shutil.which('clang++') or 'c++']
env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(ROOT / '.cache/zig-global'), ZIG_LOCAL_CACHE_DIR=str(ROOT / '.cache/zig-local'))
subprocess.run(compiler + ['-std=c++17', '-O1', '-Wall', '-Wextra', '-ffp-contract=off', '-Icore/include',
                           'core/src/CPU.cpp', 'tests/CPUTests.cpp', '-o', str(exe)],
               cwd=ROOT, env=env, check=True)
subprocess.run([str(exe)], check=True)
pe = pefile.PE(data=data)
base = pe.OPTIONAL_HEADER.ImageBase
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
u.mem_write(base, pe.get_memory_mapped_image())
mem, stack, scratch, end = 0x07000000, 0x08000000, 0x06000000, 0x09000000
u.mem_map(mem, 1048576)
for address in (stack, scratch, end): u.mem_map(address, 65536)
u.mem_write(scratch, b'\xdb\xe3')  # fninit (original default x87 environment)

def pack(values): return struct.pack('<' + 'I' * len(values), *[x & 0xffffffff for x in values])
def write(address, values): u.mem_write(address, pack(values))
def read(address, count, signed=False): return list(struct.unpack('<' + ('i' if signed else 'I') * count, u.mem_read(address, count * 4)))
def float_bits(f): return struct.unpack('<I', struct.pack('<f', f))[0]
def call(address, eax=0, ecx=0, edx=0, ebx=0, esi=0, edi=0, args=()):
    u.emu_start(scratch, scratch + 2)
    sp = stack + 60000
    write(sp, (end, *args))
    for reg, value in ((UC_X86_REG_EAX,eax),(UC_X86_REG_ECX,ecx),(UC_X86_REG_EDX,edx),(UC_X86_REG_EBX,ebx),
                       (UC_X86_REG_ESI,esi),(UC_X86_REG_EDI,edi),(UC_X86_REG_ESP,sp)):
        u.reg_write(reg, value & 0xffffffff)
    u.emu_start(address, end, count=1000000)
    assert u.reg_read(UC_X86_REG_EIP) == end, hex(address)
    return u.reg_read(UC_X86_REG_EAX)
def rng_set(words): write(0x10371fd0, words)
def rng_get(): return read(0x10371fd0, 4)

cpu, level_object, meter_object, note, chart = mem, mem + 4096, mem + 8192, mem + 12288, mem + 16384
judgments, ticks, consumed, reserved = (mem + n for n in (0x10000, 0x20000, 0x30000, 0x40000))
output0, output1, pairs, descriptor, seed_object = (mem + n for n in (0x50000, 0x50004, 0x60000, 0x70000, 0x80000))

def setup(count, level=0, last=-1, enabled=False, meter=0, override=-1, entries=None):
    u.mem_write(cpu, bytes(256))
    write(cpu + 4, (meter_object, count, level_object))
    write(cpu + 20, (last, int(enabled)))
    write(cpu + 52, (judgments, ticks, consumed, reserved, override))
    write(level_object + 20, (level,))
    write(meter_object + 100, (meter,))
    if entries is not None:
        write(judgments, [v[0] for v in entries]); write(ticks, [v[1] for v in entries])
        u.mem_write(consumed, bytes(v[2] for v in entries)); u.mem_write(reserved, bytes(v[3] for v in entries))

def state_entries(count):
    a, b = read(judgments, count, True), read(ticks, count, True)
    c, d = u.mem_read(consumed,count), u.mem_read(reserved,count)
    return [v for row in zip(a,b,c,d) for v in row]

commands, expected, modes = [], [], collections.Counter()
def case(mode, args, result):
    commands.append(mode + ' ' + ' '.join(str(v) for v in args))
    expected.append(' '.join(str(v) for v in result))
    modes[mode] += 1

generator = random.Random(0x1015dbc0)
seeds = [[0,0,0,0], [123456789,362436069,521288629,88675123], [0xffffffff,1,0x80000000,17]]
u.mem_write(cpu,bytes([0xa5])*128)
call(0x1015d8e0,eax=cpu)
assert read(cpu+20,1,True) == [-1]
assert bytes(u.mem_read(cpu+24,1)) == b'\x01'
assert read(cpu+68,1,True) == [-1]
for level in range(19):
    case('p', [level], read(0x1036c808 + level*12, 3))
for seed in (0,1,42,0x7fffffff,0x80000000,0xffffffff,*(generator.getrandbits(32) for _ in range(50))):
    call(0x10065d20, eax=seed, edx=seed_object)
    case('s',[seed],[0,*read(seed_object + 4,4)])
for i in range(300):
    words = [generator.getrandbits(32) for _ in range(4)]
    lower,upper = generator.choice(((0.0,100.0),(-100.0,100.0),(-1.0,-0.1),(0.5,0.56),(1.0,1.0),(0.99,0.995)))
    rng_set(words)
    call(0x100658e0,args=(float_bits(lower),float_bits(upper)))
    # Pop the original ST0 return into binary32 without replacing any function.
    u.mem_write(end, b'\xd9\x1d' + struct.pack('<I',output0))
    u.emu_start(end,end+6)
    case('r',[float_bits(lower),float_bits(upper),*words],[*read(output0,1),*rng_get()])

for count,level,words in itertools.product((0,1,2,3,7,91,1000,4096),range(19),seeds):
    setup(count,level)
    rng_set(words)
    call(0x1015da20,edi=cpu)
    case('d',[count,level,*words],[*read(cpu+28,5,True),*rng_get()])

for count,level,words in itertools.product((0,1,2,7,91),range(19),seeds):
    setup(count,level,entries=[(6,0,0,0)]*count)
    rng_set(words)
    call(0x1015da20,edi=cpu)
    counts = read(cpu+28,5,True)
    labels = [label for label,n in enumerate(counts) for _ in range(n)]
    assert len(labels) == count
    if count:
        write(pairs,[x for i,label in enumerate(labels) for x in (label,i)])
        write(descriptor,(1,0,0,pairs,pairs+count*8,pairs+count*8))
        write(0x1250c23c,(1,))  # existing local RNG guard: skips atexit registration only
        call(0x10005610,eax=descriptor)
        labels = read(pairs,count*2,True)[::2]
    write(judgments,labels)
    call(0x1015d990,ebx=cpu)
    case('i',[count,level,*words],[-1,*state_entries(count),*rng_get()])

def evaluate_case(entries,level,last,enabled,meter,override,words,n):
    setup(len(entries),level,last,enabled,meter,override,entries)
    u.mem_write(note,bytes(512)); u.mem_write(chart,bytes(404))
    write(note+160,(chart,)); write(chart+288,(n[1],)); write(chart+300,(n[2],));write(chart+324,(n[0],))
    write(note+192,(n[3],));write(note+360,(n[6],n[4],n[5]))
    write(output0,(123456,654321))
    rng_set(words)
    accepted = call(0x1015dbc0,eax=cpu,edi=note,args=(output0,output1)) & 255
    args = [len(entries),level,last,int(enabled),meter,override,*words,*[x for row in entries for x in row],*n]
    result = [accepted,*read(output0,2,True),*read(cpu+20,1,True),*state_entries(len(entries)),*rng_get()]
    case('e',args,result)

for kind,judgment,tick,segment,used in itertools.product((-1,0,1,4,5),(0,1,2,3,4,6),(-1,0,1,2),(-1,0,2),(0,1)):
    words = generator.choice(seeds)
    evaluate_case([(judgment,0,used,0),(2,1,0,1)],17,1,True,100,-1,words,[0,kind,0x20,tick,segment,10,10-tick])
for i in range(1000):
    count = generator.randrange(1,7)
    entries = [(generator.choice((0,1,2,3,4,6)),generator.randrange(-1,2),generator.randrange(2),generator.randrange(2)) for _ in range(count)]
    evaluate_case(entries,generator.randrange(19),generator.randrange(-1,count),bool(generator.randrange(2)),
                  generator.choice((0,99,100,101)),generator.choice((-1,0,1,25,50,100,101,2147483647)),
                  [generator.getrandbits(32) for _ in range(4)],
                  [generator.randrange(-1,count+1),generator.choice((0,1,4)),generator.choice((0,0x20,0xffffffff)),
                   generator.choice((-1,0,1,-2147483648,2147483647)),generator.choice((-1,0,1)),
                   generator.choice((0,1,-2147483648,2147483647)),generator.choice((-1,0,1))])
for level,words,override in itertools.product(range(19),seeds,(-1,0,1,50,100,101)):
    evaluate_case([(0,0,0,0)],level,-1,True,100,override,words,[0,0,0x20,0,-1,0,0])

for i in range(300):
    count = generator.randrange(0,9)
    entries = [(generator.randrange(7),generator.randrange(-1,2),generator.randrange(2),generator.randrange(2)) for _ in range(count)]
    last = generator.randrange(-1,count+2)
    judgment = generator.choice((-1,0,1,2,4,6,2147483647))
    setup(count,0,last,False,0,-1,entries)
    words = generator.choice(seeds);rng_set(words)
    index = call(0x1015d800,ecx=cpu,args=(judgment,))
    case('n',[count,0,last,0,0,-1,*words,*[x for row in entries for x in row],judgment],
         [index,*read(cpu+20,1,True),*state_entries(count),*rng_get()])

actual = subprocess.check_output([str(exe),'--batch'],input=('\n'.join(commands)+'\n').encode()).decode().splitlines()
assert len(actual) == len(expected),(len(actual),len(expected))
for i,(a,b) in enumerate(zip(actual,expected)):
    assert a == b, {'case':i,'command':commands[i],'actual':a,'original':b}
report = {'dll_sha256':digest,'cases':len(commands),'counts':dict(modes),
          'original_functions':{'constructor':'0x1015D8E0','seed':'0x10065D20','range':'0x100658E0','distribution':'0x1015DA20',
                                'shuffle':'0x10005610/0x10005750','offsets':'0x1015D990','reserve':'0x1015D800',
                                'evaluate':'0x1015DBC0','special_gate':'0x1015DB50/0x100659D0'},
          'initialization':'staged original functions; original CRT allocation/vector-growth bypassed',
          'limitations':['stage difficulty selection and chart-count extraction not restored',
                         'explicit RNG state supplied; no original startup seed selection',
                         'default x87 environment; finite CPU-use ranges; no game process or network']}
(ROOT/'.build/cpu-parity.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(f'PASS {len(commands)} original x86 CPU differential cases: {dict(modes)}')
