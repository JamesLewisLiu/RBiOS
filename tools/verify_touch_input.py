"""Post-hardware touch lifecycle original-instruction differential checks."""
from original_score_fixture import *
from unicorn.x86_const import UC_X86_REG_FPCW
u.reg_write(UC_X86_REG_FPCW,0x027f)
def call(va,args=(),**regs):
    sp=stack+60000;u.mem_write(sp,word(end)+b''.join(word(v) for v in args))
    u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EFLAGS,2)
    for name,reg in [('eax',UC_X86_REG_EAX),('ebx',UC_X86_REG_EBX),('ecx',UC_X86_REG_ECX),('edx',UC_X86_REG_EDX),('esi',UC_X86_REG_ESI),('edi',UC_X86_REG_EDI)]:u.reg_write(reg,regs.get(name,0)&0xffffffff)
    u.emu_start(va,end,count=200000)
    assert u.reg_read(UC_X86_REG_EIP)==end,'instruction limit reached'

manager=scratch+0x6000;pool=scratch+0x6800
put(manager+212,pool);put(manager+1064,2720)
put(0x1249f85c,80);put(0x1249f864,1142)
commands=[];expected=[];frames=0
for run in range(100):
    call(0x10060e50,eax=pool);nextid=2147483647 if run==0 else run*100
    put(0x12479020,nextid);commands.append(f'R {nextid}')
    points=[]
    for frame in range(30):
        for slot in range(15):
            if rng.randrange(12)==0:
                limit=rng.choice([-1.,0.,1.,20.,40.,100.]);sel=rng.randrange(2);cap=rng.randrange(2);blocked=rng.randrange(2)
                off=pool+4+64*slot;u.mem_write(off+60,struct.pack('<f',limit));u.mem_write(off+53,bytes([sel,cap]));u.mem_write(off+56,bytes([blocked]))
                commands.append(f'L {slot} {limit} {sel} {cap} {blocked}')
        points=[(max(1,min(767,x+rng.randint(-40,40))),max(10,min(1350,y+rng.randint(-40,40)))) for x,y in points if rng.randrange(5)]
        if rng.randrange(2):points.extend((rng.randrange(1,768),rng.choice([40,100,1100,1250])) for _ in range(rng.randrange(6)))
        points=points[:25]
        if frame%11==0:points=[]
        if frame==2:points=[(100,1100),(100,1101),(100,1102),(101,1100)]
        put(manager+220,len(points));u.mem_write(manager+424,bytes(25))
        for j,p in enumerate(points):u.mem_write(manager+224+8*j,struct.pack('<ii',*p))
        call(0x100619d0,args=(manager,));call(0x10061e50,args=(manager,));call(0x10061810,args=(manager,))
        vals=[struct.unpack('<i',u.mem_read(0x12479020,4))[0]]
        for i in range(15):
            r=bytes(u.mem_read(pool+4+64*i,64));vals.append(r[0]);vals.extend(struct.unpack_from('<12i',r,4));vals.extend(r[52:57]);vals.append(struct.unpack_from('<I',r,60)[0])
        commands.append('F '+str(len(points))+' '+' '.join(f'{x} {y}' for x,y in points));expected.append(' '.join(map(str,vals)));frames+=1
exe=ROOT/'.build/rbtouchinput.exe'
actual=subprocess.run([str(exe),'--batch'],input='\n'.join(commands)+'\n',capture_output=True,text=True,check=True).stdout.splitlines()
assert len(actual)==len(expected)
for i,(a,b) in enumerate(zip(actual,expected)):
    assert a.strip()==b.strip(),f'frame {i}\nport {a}\noriginal {b}'
report={'dll_sha256':digest,'x87_control_word':'0x027f','passed':True,'frames':frames,'scope':'100619D0 + 10061E50 + 10060FD0 + 10061810 + 10061770, logical point snapshots after hardware transforms/filters','limitations':['No touch hardware sampling, original 621E0/61A60 filtering, UIKit delivery timing or device validation.']}
(ROOT/'.build/touch-input-parity.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
