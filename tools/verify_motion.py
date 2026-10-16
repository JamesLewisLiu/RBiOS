"""Execute original field geometry only, isolated from Windows and network APIs."""
from pathlib import Path
import hashlib
import itertools
import json
import random
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.tools'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP

data=(ROOT.parent/'modules/reflecbeat.dll').read_bytes()
digest=hashlib.sha256(data).hexdigest()
assert digest=='ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
pe=pefile.PE(data=data);base=pe.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
u.mem_write(base,pe.get_memory_mapped_image())
scratch=0x7000000;stack=0x8000000;end=0x9000000
for address in (scratch,stack,end):u.mem_map(address,65536)

def word(value):
    return struct.pack('<f',value) if isinstance(value,float) else struct.pack('<I',value&0xffffffff)
def call(va,args=(),**regs):
    sp=stack+60000
    u.mem_write(sp,word(end)+b''.join(word(v) for v in args))
    u.reg_write(UC_X86_REG_ESP,sp)
    for name,reg in [('eax',UC_X86_REG_EAX),('ebx',UC_X86_REG_EBX),('ecx',UC_X86_REG_ECX),('edx',UC_X86_REG_EDX),('esi',UC_X86_REG_ESI),('edi',UC_X86_REG_EDI)]:
        u.reg_write(reg,regs.get(name,0)&0xffffffff)
    u.emu_start(va,end,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==end,'instruction limit reached'
    return struct.unpack('<i',word(u.reg_read(UC_X86_REG_EAX)))[0]

call(0x100a4c90)
commands=[];expected=[];counts={}
def add(category,command,result):
    counts[category]=counts.get(category,0)+1
    commands.append(command);expected.append(result)
for i in range(8):
    add('x_selector',f'X {i}',str(call(0x100a4c10,args=(i,))))
    add('y_selector',f'Y {i}',str(call(0x100a4b90,ecx=i)))
for wide,index,side in itertools.product((0,1,2),(*range(35),36,256,0xffffffff),(0,1,2)):
    call(0x100a53c0,eax=wide,ecx=index,args=(side,scratch,scratch+4))
    p=struct.unpack('<ii',u.mem_read(scratch,8))
    add('destination',f'D {wide} {index} {side}',f'{p[0]} {p[1]}')
for index,side in itertools.product((*range(8),0xffffffff),(0,1,2)):
    px=call(0x100a51b0,eax=index)
    py=call(0x100a5140,eax=index,args=(side,))
    add('lane',f'L {index} {side}',f'{px} {py}')
for x,alternate in itertools.product((-1000,-1,0,255,256,257,511,512,513,767,768,1000),(0,1)):
    add('region',f'R {x} {alternate}',str(call(0x100a50b0,eax=x,args=(alternate,))))
rng=random.Random(2019121100)
rays=list(itertools.product((36,384,732),(116,651,1186),(-1.,0.,1.),(-1.,0.,1.)))
for _ in range(500):
    rays.append((rng.randint(0,768),rng.randint(80,1222),rng.randint(-1000,1000)/16.,rng.randint(-1000,1000)/16.))
for x,y,vx,vy in rays:
    u.mem_write(scratch,struct.pack('<ii',x,y))
    b=call(0x100a4e90,ebx=y,edi=scratch,esi=scratch+4,args=(x,vx,vy))
    p=struct.unpack('<ii',u.mem_read(scratch,8))
    add('boundary',f'B {x} {y} {vx!r} {vy!r}',f'{b} {p[0]} {p[1]}')
exe=ROOT/'.build'/('rbmotion.exe' if sys.platform=='win32' else 'rbmotion')
for case in range(100):
    middle=case%4
    points=[(rng.randint(40,728),rng.randint(120,1180)) for _ in range(7)]
    if case%11==0:points[1]=points[0] # zero-length segment fallback
    order=[0,*range(1,middle+1),4]
    if case%2:order.append(5)
    order.append(6)
    ticks=rng.randint(10,150);steps=rng.randint(0,200);pause=case%5
    call(0x100a3d00,eax=scratch)
    for i,p in enumerate(points):u.mem_write(scratch+92+8*i,struct.pack('<ii',*p))
    u.mem_write(scratch+148,word(middle));u.mem_write(scratch+212,word(len(order)-1));u.mem_write(scratch+32,word(middle))
    for i,p in enumerate(order):u.mem_write(scratch+156+4*i,word(p))
    call(0x100a42d0,edi=scratch,args=(ticks,))
    vals=[struct.unpack('<I',u.mem_read(scratch+312,4))[0],struct.unpack('<I',u.mem_read(scratch+8,4))[0]]
    vals.extend(struct.unpack('<'+'I'*(16*(len(order)-1)//4),u.mem_read(scratch+216,16*(len(order)-1))))
    vals.extend(struct.unpack('<8I',u.mem_read(scratch+324,32)))
    u.mem_write(scratch+16,bytes([pause>0]));u.mem_write(scratch+20,word(pause))
    note=scratch+4096;suppress=case%3==0
    u.mem_write(note,bytes(448));u.mem_write(note+212,word(scratch));u.mem_write(note+427,bytes([1]));u.mem_write(note+424,bytes([suppress]))
    u.mem_write(note+168,struct.pack('<2f',*points[0]));u.mem_write(note+100,struct.pack('<2i',*points[0]))
    for _ in range(steps):
        call(0x1015a240,eax=note) # offscreen-reported flag bypasses external callback only
        call(0x100a3ea0,ecx=scratch)
    vals.extend(struct.unpack('<2I',u.mem_read(scratch+316,8)))
    v=call(0x100a3e40,eax=scratch)
    vals.extend(struct.unpack('<2I',u.mem_read(v&0xffffffff,8)))
    vals.extend(struct.unpack('<4I',u.mem_read(scratch,16))[:2])
    # +8 is segment count, not tick. Output current/previous index, tick, pause.
    vals.append(struct.unpack('<i',u.mem_read(scratch+12,4))[0]);vals.append(int(bool(u.mem_read(scratch+16,1)[0])))
    vals.append(struct.unpack('<i',u.mem_read(scratch+20,4))[0])
    vals.extend(struct.unpack('<6I',u.mem_read(note+168,24)))
    vals.extend(struct.unpack('<2i',u.mem_read(note+100,8)));vals.append(int(bool(u.mem_read(note+424,1)[0])))
    cmd=f'V {ticks} {middle} {middle} {len(order)} '+' '.join(f'{x} {y}' for x,y in points)+' '+' '.join(map(str,order))+f' {steps} {pause} {int(suppress)}'
    add('velocity_timeline',cmd,' '.join(map(str,vals)))
actual=subprocess.check_output([str(exe),'--batch'],input=('\n'.join(commands)+'\n').encode()).decode().splitlines()
assert len(actual)==len(expected)
for command,wanted,result in zip(commands,expected,actual):
    if wanted.strip()!=result.strip():raise AssertionError((command,wanted,result))
report={'dll_sha256':digest,'method':'bounded original x86 emulation; no Windows APIs or network','cases':sum(counts.values()),'coverage':counts,'limitations':['original default field only; runtime-adjusted offsets not differentially covered','supplied route points; automatic path construction, offscreen callbacks and complete note state machine not restored','nonfinite velocities rejected by adapter; not compared to undefined original behavior']}
(ROOT/'.build/motion-parity.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report))
