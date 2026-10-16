"""Execute only isolated original arithmetic in Unicorn, with no OS or network."""
from pathlib import Path
import hashlib
import json
import itertools
import random
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.tools'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EDX,UC_X86_REG_ECX,UC_X86_REG_ESP

dll=ROOT.parent/'modules/reflecbeat.dll';data=dll.read_bytes()
expected='ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
digest=hashlib.sha256(data).hexdigest()
if digest!=expected:raise SystemExit('binary identity mismatch')
pe=pefile.PE(data=data);base=pe.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
state=0x07000000;stack=0x08000000;end=0x09000000
for address in [state,stack,end]:u.mem_map(address,65536)
def call(address,eax=0,edx=state,ecx=0,args=()):
    sp=stack+60000
    u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),end,*[x&0xffffffff for x in args]))
    u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EAX,eax&0xffffffff);u.reg_write(UC_X86_REG_EDX,edx);u.reg_write(UC_X86_REG_ECX,ecx)
    u.emu_start(address,end,count=10000);return u.reg_read(UC_X86_REG_EAX)
exe=ROOT/'.build'/('rbinspect.exe' if sys.platform=='win32' else 'rbinspect')
cases=0
for seed in list(range(32))+[0xffffffff,0x80000000,0x7fffffff,123456789]:
    call(0x10065d20,seed)
    actual=struct.unpack('<4I',u.mem_read(state+4,16))
    restored=tuple(map(int,subprocess.check_output([str(exe),'--seed',str(seed)]).split()))
    if actual!=restored:raise AssertionError((seed,actual,restored))
    cases+=1
holdcases=0
for ms in [0,1,16,17,483,499,500,516,517,666,667,1000,1001,30000,32767]:
    u.mem_write(state,bytes(404));u.mem_write(state+288,struct.pack('<I',1));u.mem_write(state+344,struct.pack('<i',int(ms*60.0/1000)))
    actual=call(0x10149aa0,state)
    restored=int(subprocess.check_output([str(exe),'--long-ms',str(ms)]))
    if actual!=restored:raise AssertionError((ms,actual,restored))
    holdcases+=1
normal=((9,9,5,3,-2,-5,-9,-9),(9,9,4,1,-1,-4,-9,-9))
alternate=((9,9,5,4,-3,-5,-9,-9),(9,9,5,2,-2,-5,-9,-9))
assert struct.unpack('<16i',pe.get_data(0x2385b0,64))==sum(normal,())
assert struct.unpack('<16i',pe.get_data(0x2385f0,64))==sum(alternate,())
rule_inputs=[];rule_expected=[]
switchcases=0
for start,travel in itertools.product((-4000,-2500,0,1001),(0,1000)):
    for segments in ((),((0,500),),((0,499),(499,501)),((0,500),(500,500),(1000,1000))):
        u.mem_write(state,bytes(404));u.mem_write(state+288,struct.pack('<I',4))
        u.mem_write(state+316,struct.pack('<ii',max(-150,int(start*60.0/1000)),int(travel*60.0/1000)))
        extra=state+8192
        u.mem_write(state+392,struct.pack('<II',extra,len(segments)))
        for i,(time,duration) in enumerate(segments):
            u.mem_write(extra+28*i,bytes(28));u.mem_write(extra+28*i+8,struct.pack('<ii',time,duration))
        actual=call(0x10149aa0,state)
        rule_inputs.append(f'S {start} {travel} {len(segments)} '+ ' '.join(f'{t} {d}' for t,d in segments))
        rule_expected.append(str(actual));switchcases+=1
timingcases=0
for mode,alt,reject in itertools.product((0,1,2,0xffffffff),(0,1),(0,1)):
    call(0x1016aca0,mode,args=(reject,))
    for tick in [*range(-16,17),-2147483648,2147483647]:
        actual=call(0x1016ac10,tick,args=(alt,reject))
        rule_inputs.append(f'T {tick} {mode} {alt} {reject}')
        rule_expected.append(str(actual));timingcases+=1
actor=state;chart=state+4096
rng=random.Random(2019121100)
note_cases=[]
# Exact boundaries plus deterministic varied states, including x86 wraparound.
for kind,relative,elapsed in itertools.product((0,1,4),(-1001,-1000,-45,-25,-16,-15,0,1),(14,15,16,25,30)):
    note_cases.append((kind,relative,60,elapsed,0,30,0))
for _ in range(300):
    note_cases.append((rng.choice((0,1,4)),rng.randint(-300,100),rng.randint(0,300),rng.randint(0,300),rng.randint(-1,3),rng.randint(0,300),rng.randint(0,1)))
for value in (-2147483648,2147483647):
    for kind in (1,4):note_cases.append((kind,value,value,value,0,value,0))
for kind,relative,duration,elapsed,index,segment,ended in note_cases:
    u.mem_write(actor,bytes(448));u.mem_write(chart,bytes(404))
    u.mem_write(actor+160,struct.pack('<I',chart));u.mem_write(chart+288,struct.pack('<i',kind))
    for off,value in ((192,relative),(296,duration),(360,elapsed),(364,index),(368,segment)):
        u.mem_write(actor+off,struct.pack('<i',value))
    u.mem_write(actor+376,bytes([ended]))
    values=(call(0x10158640,actor)&255,call(0x10158700,ecx=actor)&255,call(0x101587c0,ecx=actor)&255)
    rule_inputs.append(f'H {kind} {relative} {duration} {elapsed} {index} {segment} {ended}')
    rule_expected.append(' '.join(map(str,values)))
restored=subprocess.check_output([str(exe),'--rules'],input=('\n'.join(rule_inputs)+'\n').encode()).decode().splitlines()
assert len(restored)==len(rule_expected)
for args,actual,recovered in zip(rule_inputs,rule_expected,restored):
    if actual!=recovered:raise AssertionError((args,actual,recovered))
report={'dll_sha256':digest,'method':'bounded isolated x86 emulation, no Windows APIs or network','random_seed_cases':cases,'long_tick_cases':holdcases,'switch_long_tick_cases':switchcases,'timing_classification_cases':timingcases,'note_timing_cases':len(note_cases),'limitations':['does not validate complete gameplay, CPU, trajectories, judgment dispatch or score','synthetic switch segments do not validate their gameplay transitions']}
(ROOT/'.build/recovered-parity.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report))
