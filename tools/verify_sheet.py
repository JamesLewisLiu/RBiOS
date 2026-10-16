"""Original sheet loader / preprocessing in isolated x86, no game process.

CRT allocation and vector construction use explicit bounded memory
fixtures. Constructors are sampled from their original x86 and copied; the
original loader, raw parser, counts and destination algorithms execute.
"""
from original_score_fixture import *
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_FPCW

u.mem_map(0,65536)
heap_base=0xa000000
u.mem_map(heap_base,16*1024*1024)
sheet=scratch+0x8000
payload=scratch+0xa000
settings=scratch+0x5000
u.reg_write(UC_X86_REG_FPCW,0x027f)
heap=heap_base
templates={}
for va,size in [(0x10149850,404),(0x100a3700,28)]:
    u.mem_write(sheet,bytes(512));call(va,ecx=sheet)
    templates[va]=bytes(u.mem_read(sheet,size))

def read(address):return struct.unpack('<I',u.mem_read(address,4))[0]
def si(address):return struct.unpack('<i',u.mem_read(address,4))[0]
def ret(value=None,pop=0):
    sp=u.reg_read(UC_X86_REG_ESP)
    if value is not None:u.reg_write(UC_X86_REG_EAX,value&0xffffffff)
    u.reg_write(UC_X86_REG_EIP,read(sp));u.reg_write(UC_X86_REG_ESP,sp+4+pop)
def hook(uc,address,size,user):
    global heap
    sp=u.reg_read(UC_X86_REG_ESP)
    if address in (0x1002f218,0x1002f35d):
        amount=read(sp+4);assert amount<2*1024*1024
        pointer=heap;heap+=(max(1,amount)+15)&~15
        assert heap<heap_base+16*1024*1024
        u.mem_write(pointer,bytes(amount));ret(pointer)
    elif address in (0x1002f427,0x1002f29a,0x1002f337,0x1003005b):ret()
    elif address==0x10030072:
        pointer,stride,count,ctor=[read(sp+4*i) for i in range(1,5)]
        template=templates[ctor];assert len(template)==stride
        u.mem_write(pointer,template*count);ret(pop=20)
u.hook_add(UC_HOOK_CODE,hook)

def invoke(va,args=(),**regs):
    sp=stack+60000;u.mem_write(sp,word(end)+b''.join(word(v) for v in args))
    u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EFLAGS,2)
    for name,reg in [('eax',UC_X86_REG_EAX),('ebx',UC_X86_REG_EBX),('ecx',UC_X86_REG_ECX),('edx',UC_X86_REG_EDX),('esi',UC_X86_REG_ESI),('edi',UC_X86_REG_EDI)]:u.reg_write(reg,regs.get(name,0)&0xffffffff)
    try:u.emu_start(va,end,count=30000000)
    except Exception as e:raise RuntimeError(f'{hex(va)} failed at {hex(u.reg_read(UC_X86_REG_EIP))}: {e}') from e
    assert u.reg_read(UC_X86_REG_EIP)==end, f'instruction bound {hex(va)} at {hex(u.reg_read(UC_X86_REG_EIP))}'
    return u.reg_read(UC_X86_REG_EAX)

def original(raw,side,difficulty,seed,prepare=False):
    global heap
    heap=heap_base
    put(0x1250c3b0,0) # original temporary shuffle vector is allocated per fixture arena
    # Raw input lives in its own arena and remains immutable throughout loader.
    source=heap_base+14*1024*1024
    u.mem_write(source,raw)
    u.mem_write(sheet,bytes(512));invoke(0x1014a960,esi=sheet)
    put(sheet+4,20);put(sheet+164,seed)
    put(0x10a71058,config);u.mem_write(config,bytes(12000))
    u.mem_write(settings,bytes(1792));put(settings+4,6);put(settings+12,difficulty);put(settings+20,side)
    assert invoke(0x1014a230,args=(sheet,source+16,settings))&255
    assert invoke(0x10149e90,esi=sheet,args=(settings,))&255
    if prepare:assert invoke(0x1014b270,edi=sheet,args=(settings,))&255
    values=[]
    for off in [88,92,96,100,104,108,112,116,120,124]:values.append(si(sheet+off))
    count=si(sheet+64);base=read(sheet+140)
    # Per-side object counts are the loader-assigned ordinal maximum +1.
    counts=[0,0]
    for i in range(count):
        at=base+i*404; color=si(at+280);color=color if 0<=color<=1 else 0
        counts[color]=max(counts[color],si(at+324)+1)
    values+=counts+[si(sheet+o) for o in (128,132,136)]
    for i in range(count):
        at=base+i*404
        values += [si(at+o) for o in (276,280,284,288,316,320,324,328,332,344,348,352,400)]
        values += [struct.unpack('<h',u.mem_read(at+o,2))[0] for o in (304,306)]
        values += [si(at+o) for o in (340,360,364,368,372,388)]
        values += [u.mem_read(at+357,1)[0],si(at+396)]
        values += list(u.mem_read(at+376,10))
    return values

if __name__=='__main__':
    exe=ROOT/'.build/rbmusicsheet.exe'
    paths=sorted((ROOT.parent/'data/sound/ply2').glob('*.ply'))[:3]
    commands=[];expected=[]
    for prepare in (False,True):
        for path in paths:
            raw=path.read_bytes()
            for side in (0,1):
                values=original(raw,side,2,77,prepare)
                commands.append(f'{"P" if prepare else "L"} {side} 2 77 {raw.hex()}')
                expected.append(values)
    actual=subprocess.check_output([str(exe),'--batch'],input=('\n'.join(commands)+'\n').encode()).decode().splitlines()
    assert len(actual)==len(expected),(len(actual),len(expected))
    for i,(want,line) in enumerate(zip(expected,actual)):
        got=list(map(int,line.split()))
        if want!=got:
            differences=[(j,a,b) for j,(a,b) in enumerate(zip(want,got)) if a!=b]
            raise AssertionError((i,len(want),len(got),differences[:25]))
    report={'dll_sha256':digest,'x87_control_word':'0x027f','cases':len(commands),
            'resources':[p.name for p in paths], 'fields_compared':sum(map(len,expected)),
            'scope':'original loader/counts/preparation; both chart sides; default destination option',
            'fixtures':'bounded CRT allocation and vector construction; original constructors sampled; qsort executes original code'}
    (ROOT/'.build/sheet-parity.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS',len(commands),'original sheet comparisons')
