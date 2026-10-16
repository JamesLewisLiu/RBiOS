"""Shipped CRT qsort + original stage comparator against native StageOrder.
No DLL entry point, game process, Windows API, or transport is executed.
"""
import json,random,struct,subprocess,math
from pathlib import Path
from original_score_fixture import u,scratch,stack,end,word,put,digest,note,call
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EFLAGS,UC_X86_REG_EIP,UC_X86_REG_FPCW
u.reg_write(UC_X86_REG_FPCW,0x027f)
ROOT=Path(__file__).resolve().parents[1]
u.mem_map(scratch+0x10000,0x20000)
array=scratch+0x8000;notes=scratch+0x10000
rng=random.Random(10157900);cases=[];expected=[]
for case in range(1000):
 n=256 if case<800 else rng.randrange(1,257)
 values=[(rng.random()<rng.choice([0,.1,.5,.9,1]),rng.randrange(-10,11)) for _ in range(n)]
 for i,(active,tick) in enumerate(values):
  address=notes+256*i;put(array+4*i,address);u.mem_write(address+20,bytes([active]));put(address+192,tick)
 sp=stack+60000;u.mem_write(sp,word(end)+word(array)+word(n)+word(4)+word(0x10157900))
 u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EFLAGS,2)
 u.emu_start(0x100302d0,end,count=500000)
 assert u.reg_read(UC_X86_REG_EIP)==end
 expected.append([(x-notes)//256 for x in struct.unpack('<'+'I'*n,u.mem_read(array,4*n))])
 cases.append(str(n)+' '+' '.join(f'{int(a)} {t}' for a,t in values))
p=subprocess.run([str(ROOT/'.build/rbstagesession.exe'),'--sort'],input='\n'.join(cases)+'\n',text=True,capture_output=True,check=True)
for case,(line,want) in enumerate(zip(p.stdout.splitlines(),expected)):
 actual=list(map(int,line.split()))
 if actual!=want:raise AssertionError((case,actual,want))
assert len(p.stdout.splitlines())==len(expected)
def float32(v):return struct.unpack('<f',struct.pack('<f',v))[0]
for case in range(1000):
 x,y=(float32(rng.uniform(-100,100)) for _ in range(2))
 u.mem_write(note+176,struct.pack('<ff',x,y));call(0x1015a100,ecx=note)
 got=struct.unpack('<f',u.mem_read(note+272,4))[0]
 want=float32(float32(math.atan2(-1.,0.))-float32(math.atan2(-y,x)))
 assert got==want,(case,x,y,got,want)
report={'dll_sha256':digest,'stage_comparator':'10157900','crt_sort':'100302D0/10030240','cases':len(cases),'direction_1015A100_cases':1000,'x87_control_word':'0x027f','passed':True}
(ROOT/'.build/stage-order-parity.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report))
