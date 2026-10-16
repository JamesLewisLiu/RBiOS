"""Isolated original CNoteBase initialization/tail/frame-suffix comparisons.

Presentation effect 101589B0 is replaced with a counted fixture. Frame-suffix
motion advance is an explicit fixture, and deletion is suppressed by its query;
the numeric state, scheduling requests and countdowns still execute original x86.
"""
from original_score_fixture import *
from unicorn.x86_const import UC_X86_REG_FPCW
u.reg_write(UC_X86_REG_FPCW, 0x027f) # original startup selects 53-bit x87 precision
path, child = scratch+0x9000, scratch+0x9400
effect=end+0x900
u.mem_write(effect,bytes(4))
# Original exit state's hold-presentation effect is outside the logical core.
u.mem_write(0x101589b0,b'\xff\x05'+word(effect)+b'\xc3')
u.ctl_remove_cache(0x101589b0,0x101589c0)
u.mem_write(0x100a3ea0,b'\xc3')
u.ctl_remove_cache(0x100a3ea0,0x100a3eb0)
u.mem_write(end+0x500,b'\x31\xc0\xc3')
addresses={'I':0x10159090,'L':0x1015a1a0,'E':0x10159b30,'R':0x10159af0,'F':0x1015ac00,'S':0x1015b720}
commands=[]; expected=[];counts={}
def write32(raw,offset,value):struct.pack_into('<i',raw,offset,value)
def setfloat(raw,offset,value):struct.pack_into('<f',raw,offset,value)
for op in addresses:
    counts[op]=0
    for iteration in range(500):
        raw=bytearray(1220)
        for offset in [100,104,192,196,200,204,208,296,300,316,320,344,348,360,368,420]:
            write32(raw,offset,rng.randint(-100,100))
        for offset in [116,128]:write32(raw,offset,rng.choice([0,0x10159b30,0x1015a1a0]))
        for offset in [132,136,144,148]:write32(raw,offset,rng.choice([0,1,2,3,4,5,9,30]))
        for offset in [20,140,141,308,309,376,409,410,411,426,427]:raw[offset]=rng.randrange(2)
        for offset in [332,336]:setfloat(raw,offset,rng.choice([-123.5,-50.25,0.,1.125,42.,67.75,100.]))
        setfloat(raw,280,rng.choice([0.,.01,.15,.3,.5,.75,1.,1.5]))
        write32(raw,364,rng.choice([-1,0,1,2,3]));write32(raw,448+396,rng.choice([0,1,2,3]))
        write32(raw,448+288,rng.choice([0,1,2,3,4]));write32(raw,448+328,rng.randrange(2))
        write32(raw,448+300,rng.choice([0,0x10]));write32(raw,448+316,rng.randint(-150,300))
        write32(raw,448+320,rng.randint(10,150));write32(raw,448+344,rng.randint(0,300))
        write32(raw,864+40,rng.choice([0,1,2]));write32(raw,864+44,rng.randrange(2))
        if op=='I' and struct.unpack_from('<i',raw,448+288)[0]==4:write32(raw,448+288,1)
        if op=='E':write32(raw,296,rng.choice([1,7,30,60,90,300]))
        tick=rng.randint(-150,600);remote_session=rng.randrange(2)
        # Pointers are identity fields only and never copied into native objects.
        write32(raw,0,vtable);write32(raw,160,chart);write32(raw,212,path)
        if op=='F' and struct.unpack_from('<i',raw,448+288)[0]==4:write32(raw,264,child)
        u.mem_write(note,bytes(raw[:448]));u.mem_write(chart,bytes(raw[448:864]));u.mem_write(path,bytes(raw[864:]))
        put(0x12479308,tick);u.mem_write(remote+12872,bytes([remote_session]))
        put(vtable+100,end+0x500);put(effect,0)
        call(addresses[op],ecx=note,args=(0,) if op in 'LER' else ())
        result=bytearray(u.mem_read(note,448))
        if op=='S':write32(result,112,0)
        commands.append(op+' '+raw.hex()+f' {tick} {remote_session}')
        expected.append(result.hex()+' '+str(int(struct.unpack('<I',u.mem_read(effect,4))[0]!=0)))
        counts[op]+=1
# Exercise scheduler plus ordinary route/travel states against original motion.
u.mem_write(0x100a3ea0,pe.get_memory_mapped_image()[0xa3ea0:0xa3ec0])
u.ctl_remove_cache(0x100a3ea0,0x100a3ec0)
u.mem_write(0x10159ca0,b'\xc3');u.ctl_remove_cache(0x10159ca0,0x10159cb0)
put(vtable+156,end+0x500);put(vtable+176,0x10158220)
u.mem_write(end+0xa00,struct.pack('<f',120.))
u.mem_write(0x10148e80,b'\xd9\x05'+word(end+0xa00)+b'\xc3');u.ctl_remove_cache(0x10148e80,0x10148e90)
u.mem_write(0x100a42d0,b'\xc2\x04\x00');u.ctl_remove_cache(0x100a42d0,0x100a42e0)
u.mem_write(0x1247932c,bytes(4))
call(0x100a4c90)
phases=[0x10159af0,0x10159b30,0x1015a1a0,0x1015a470,0x1015a5e0,0x1015a540,0x1015a740,0x1015b0d0,0x1015b250,0x1015b3f0,0x1015b590]
counts['A']=0
for iteration in range(1500):
    raw=bytearray(1220);phase=rng.choice(phases)
    for offset,value in [(0,vtable),(112,note),(116,phase),(160,chart),(212,path),
                         (192,rng.randint(-150,150)),(196,-150),(200,0),(296,rng.randint(1,180)),
                         (300,rng.randint(-100,100)),(324,rng.randint(0,80)),(364,rng.choice([-1,0,1])),
                         (368,90),(360,rng.randint(0,150)),(448+396,2),
                         (448+288,1 if phase in [0x1015a5e0,0x1015a540,0x1015b250] else 4 if phase in [0x1015a470,0x1015b0d0] else 0),
                         (448+284,rng.randrange(2)),(448+316,rng.randint(0,100)),(864+8,1),(864+228,1000)]:write32(raw,offset,value)
    raw[20]=1;raw[141]=1;raw[378]=1;raw[427]=1
    if iteration%3==0:write32(raw,128,rng.choice(phases))
    raw[140]=rng.randrange(2)
    for offset in [308,309,340,328,409,410]:raw[offset]=rng.randrange(2)
    for offset in [132,136,144,148]:write32(raw,offset,rng.choice([0,1,2,3,4,5,30]))
    write32(raw,100,rng.randint(0,768));write32(raw,104,rng.randint(80,1222))
    for offset in [316,320,344,348,352,356]:write32(raw,offset,rng.randint(0,768))
    for offset in [332,336]:setfloat(raw,offset,rng.choice([-123.5,-50.25,0.,1.125,42.,67.75,100.]))
    setfloat(raw,280,rng.choice([0.,.15,.5,1.]))
    setfloat(raw,168,float(struct.unpack_from('<i',raw,100)[0]));setfloat(raw,172,float(struct.unpack_from('<i',raw,104)[0]))
    for offset in [176,180,184,188,864+216,864+220,864+316,864+320]:setfloat(raw,offset,rng.choice([-3.25,-1.,0.,1.25,4.75]))
    setfloat(raw,864+312,10.)
    u.mem_write(note,bytes(raw[:448]));u.mem_write(chart,bytes(raw[448:864]));u.mem_write(path,bytes(raw[864:]))
    put(effect,0);put(0x12479308,50)
    call(0x10008d60,esi=note+112)
    result=bytes(u.mem_read(note,448))
    commands.append('A '+raw.hex()+' 50 0')
    expected.append(result.hex()+' '+str(int(struct.unpack('<I',u.mem_read(effect,4))[0]!=0)))
    counts['A']+=1
exe=ROOT/'.build'/('rbnoteframe.exe' if sys.platform=='win32' else 'rbnoteframe')
actual=subprocess.check_output([str(exe),'--batch'],input=('\n'.join(commands)+'\n').encode()).decode().splitlines()
assert len(actual)==len(expected)
for index,(wanted,got) in enumerate(zip(expected,actual)):
    if wanted!=got:
        a=bytes.fromhex(wanted.split()[0]);b=bytes.fromhex(got.split()[0])
        raw=bytes.fromhex(commands[index].split()[1])
        raise AssertionError((index,commands[index][:1],hex(struct.unpack_from('<I',raw,116)[0]),[(i,x,y) for i,(x,y) in enumerate(zip(a,b)) if x!=y],[(o,struct.unpack_from('<f',a,o)[0],struct.unpack_from('<f',b,o)[0]) for o in [332,336]],wanted.split()[1],got.split()[1]))
report={'dll_sha256':digest,'x87_control_word':'0x027f','cases':len(commands),'groups':counts,
 'method':'original x86 instructions, pointer-free native state adapter; byte-exact full 448-byte note state comparison',
 'limitations':['hold-presentation effect 101589B0 is counted fixture',
 'frame suffix motion and deletion hooks are explicit fixtures, kind-4 presentation child is scratch memory',
 'initialization excludes linked-note lookup and preprocessed switch duration',
 'ordinary route states run with ready paths; remote/special route hooks not covered',
 'top-entry tempo lookup is fixed 120 BPM and timeline reset is an explicit fixture; ramp and note state execute original instructions']}
(ROOT/'.build/frame-parity.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(report))
