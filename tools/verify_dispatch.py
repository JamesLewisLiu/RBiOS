"""Original dispatcher differential tests with explicit input-query fixtures.

The dispatcher and its decision helpers execute original x86. Touch hit/hold and
miss queries are injected at their function boundaries, separately validated by
verify_touch.py. Replay lookup/vector logic executes original code with a bounded
in-memory allocator. CPU-present paths require separate CPU coverage.
"""
from pathlib import Path
import hashlib
import json
import random
import struct
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.tools'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP)
data=(ROOT.parent/'modules/reflecbeat.dll').read_bytes()
digest=hashlib.sha256(data).hexdigest()
assert digest=='ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
pe=pefile.PE(data=data);base=pe.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
scratch,stack,end=0x7000000,0x8000000,0x9000000
for a in (0,scratch,stack,end):u.mem_map(a,65536)
note,chart,manager,out,vtable=scratch,scratch+0x400,scratch+0x800,scratch+0xc00,scratch+0x1000
def word(v):return struct.pack('<I',v&0xffffffff)
def put(a,v):u.mem_write(a,word(v))
def read(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def signed(v):return struct.unpack('<i',word(v))[0]
def call(va,args=(),**regs):
    sp=stack+60000;u.mem_write(sp,word(end)+b''.join(word(v) for v in args));u.reg_write(UC_X86_REG_ESP,sp)
    for name,reg in [('eax',UC_X86_REG_EAX),('ebx',UC_X86_REG_EBX),('ecx',UC_X86_REG_ECX),('edx',UC_X86_REG_EDX),('esi',UC_X86_REG_ESI),('edi',UC_X86_REG_EDI)]:
        u.reg_write(reg,regs.get(name,0)&0xffffffff)
    u.emu_start(va,end,count=20000)
    assert u.reg_read(UC_X86_REG_EIP)==end,'instruction limit reached'
    return u.reg_read(UC_X86_REG_EAX)&0xff
def return_query(value,pop=0):
    sp=u.reg_read(UC_X86_REG_ESP);ret=read(sp)
    u.reg_write(UC_X86_REG_EAX,value&0xffffffff)
    u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,ret)
fixture={}
heap=scratch+0x4000
def hook(uc,address,size,user):
    global heap
    if address==0x1002f35d:
        amount=read(u.reg_read(UC_X86_REG_ESP)+4)
        assert 0<amount<=4096 and heap+amount<scratch+65536
        allocated=heap;heap+=(amount+15)&~15;u.mem_write(allocated,b'\0'*amount)
        return_query(allocated)
    elif address==0x1002f337:return_query(0)
    elif address==0x10158550:return_query(fixture['miss'])
    elif address==0x10159820:return_query(fixture['press'])
    elif address==0x10158ba0:return_query(fixture['hold'])
    elif address==0x10159d10:return_query(fixture['switch_held'],4)
    elif address==0x10159610:
        value=fixture['switch_press'];sp=u.reg_read(UC_X86_REG_ESP)
        if value>=0:put(read(sp+4),value)
        return_query(value>=0,4)
u.hook_add(UC_HOOK_CODE,hook)
put(note,vtable);put(note+160,chart);put(0x12479290,manager)
put(vtable+188,0x10158550);put(vtable+184,0x10159820)
rng=random.Random(2019121100);commands=[];expected=[];counts={}
replay_cases=2000
for i in range(8000+replay_cases):
    heap=scratch+0x4000
    kind=rng.choice([0,1,2,3,4,5]);tick=rng.choice([-1001,-1000,-500,-16,-15,-10,-9,-5,-3,-2,-1,0,1,2,3,4,5,9,10,15,30])
    duration=rng.choice([0,30,60,100]);elapsed=rng.choice([-1,0,1,9,15,25,30,40,60]);segment=rng.choice([-1,0,1,2])
    segduration=rng.choice([0,30,60]);ended=rng.randrange(2);completed=rng.randrange(2);held=rng.randrange(2)
    style=rng.choice([0,1,6]);contact=rng.choice([-1,0,12,999]);switches=rng.randrange(4);direction=rng.randrange(2)
    released=rng.randrange(2);cursor=-1;mode=rng.choice([0,1,2,3,4,5,6,-1]);table=rng.randrange(2);reject=rng.randrange(2)
    codes=None
    if i>=8000:
        mode=3;cursor=rng.choice([-1,0,1,2,3]);codes=[rng.randrange(7) for _ in range(rng.randrange(9))]
        replay=scratch+0x2000;records=scratch+0x2200
        put(replay+64,records);u.mem_write(replay+68,struct.pack('<h',len(codes)))
        u.mem_write(records,b''.join(struct.pack('<HBB',0,0,c) for c in codes))
    miss,press,hold,sh=[rng.randrange(2) for _ in range(4)];sp=rng.choice([-1,5,7])
    fixture.update(miss=miss,press=press,hold=hold,switch_held=sh,switch_press=sp)
    for offset,n in [(192,tick),(296,duration),(360,elapsed),(364,segment),(368,segduration),(312,style),(216,contact),(372,direction)]:put(note+offset,n)
    for offset,n in [(376,ended),(308,completed),(309,held),(409,released),(417,cursor&255)]:u.mem_write(note+offset,bytes([n]))
    put(chart+288,kind);put(chart+396,switches);put(chart+328,0)
    call(0x1016aca0,eax=table,args=(reject,))
    put(out,6);put(out+4,-1)
    found=bool(call(0x1016b940,ecx=0 if codes is None else replay,edx=mode,args=(note,out,out+4)))
    grade,reason=(signed(read(out)),signed(read(out+4))) if found else (6,-1)
    afterheld=u.mem_read(note+309,1)[0];aftercontact=signed(read(note+216))
    values=[kind,tick,duration,elapsed,segment,segduration,ended,completed,held,style,contact,switches,direction,released,cursor,mode,table,reject,miss,press,hold,sp,sh,-1 if codes is None else len(codes)]
    if codes is not None:values.extend(codes)
    commands.append(' '.join(map(str,values)));expected.append(f'{int(found)} {grade} {reason} {afterheld} {aftercontact}')
    counts[str(mode)]=counts.get(str(mode),0)+1
exe=ROOT/'.build'/('rbjudgedispatch.exe' if sys.platform=='win32' else 'rbjudgedispatch')
actual=subprocess.check_output([str(exe),'--batch'],input=('\n'.join(commands)+'\n').encode()).decode().splitlines()
assert len(actual)==len(expected),(len(actual),len(expected))
for i,(command,wanted,got) in enumerate(zip(commands,expected,actual)):
    if wanted!=got:raise AssertionError((i,command,wanted,got))
report={'dll_sha256':digest,'cases':len(commands),'mode_cases':counts,
    'replay_present_cases':replay_cases,
    'method':'original x86 dispatcher and decision helpers; touch query boundaries use explicit boolean/reason fixtures',
    'limitations':['CPU object absent in dispatcher differential suite; CPU.evaluate separately verified',
                   'touch/miss query internals require separate verify_touch.py coverage',
                   'does not validate stage event side effects, rendering or audio']}
(ROOT/'.build/dispatch-parity.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report))
