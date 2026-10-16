"""Compare logical base-note submission and stage scoring with original x86.

Presentation virtuals are explicit no-op fixtures. Original note mutation,
judgment normalization, per-event scoring and completion bonus all execute.
This does not claim to validate AFP actor lifetime, visuals or sounds.
"""
from original_score_fixture import *
stage,stage_vtable,song,extras=scratch+0x8000,scratch+0x8400,scratch+0x8800,scratch+0x8c00
put(note,vtable);put(vtable+144,0x10159e10);put(vtable+180,0x101581d0);put(vtable+196,0x10005d00)
put(stage,stage_vtable);put(stage+80,manager);put(stage+200,manager);put(stage+60,song);put(stage+212,1)
put(stage_vtable+72,0x1015edd0)
for offset,pop in [(36,12),(68,12),(76,8),(80,8),(84,12)]:
    address=end+0x400+offset*16
    u.mem_write(address,b'\x31\xc0\xc2'+struct.pack('<H',pop));put(stage_vtable+offset,address)
put(chart+392,extras)
commands,expected=[],[]
for iteration in range(1500):
    raw=fixture();load(raw)
    kind=rng.choice([0,1,2,3,4]);tick=rng.randint(-100,20);duration=rng.choice([0,30,60])
    flags=rng.choice([0,8]);cursor=rng.choice([-1,0,1,2,126,127]);ended=rng.randrange(2)
    success=rng.randrange(2);blocked=rng.randrange(2);cooldown=rng.randrange(3);deviation=rng.randint(-9,9)
    style=rng.choice([0,1,6]);count=rng.choice([0,1,2,3]);segment=rng.randint(-1,count)
    elapsed=rng.randint(0,90);segduration=rng.choice([0,30,60]);direction=rng.randrange(2)
    completed=rng.randrange(2);held=rng.randrange(2);contact=rng.choice([-1,1,42]);side=rng.randrange(2)
    penalty=rng.randrange(2);special=rng.randrange(2);grade=rng.randrange(5);reason=rng.choice([0,1,2,3,4,5,6,7,8,11,12,13])
    disabled=rng.randrange(2);mode=rng.randrange(2);diff=rng.randrange(4);gauge=rng.randrange(5);option=rng.randrange(6)
    durations=[rng.choice([30,60,90]) for _ in range(count)]
    a=struct.unpack_from('<5i',raw,172);done=sum(a)
    i32(raw,256,done+rng.choice([0,1,50]));i32(raw,260,0)
    load(raw)
    put(song+88,struct.unpack_from('<i',raw,256)[0])
    # Keep medal actor absent through stageModeSix for cases that could invoke it;
    # medal arithmetic already has 300 full original comparisons in verify_score.
    if reason not in (7,8) or grade==3:
        predicted=done+1
        if predicted>=struct.unpack_from('<i',raw,256)[0]:mode=1
    put(config+8,6 if mode else 0);put(config+16,diff);put(config+1080,option)
    for offset,n in [(192,tick),(296,duration),(420,cooldown),(412,deviation),(312,style),
                     (364,segment),(360,elapsed),(368,segduration),(372,direction),(216,contact)]:put(note+offset,n)
    for offset,n in [(417,cursor&255),(376,ended),(377,success),(411,blocked),(308,completed),
                     (309,held),(426,penalty),(379,special)]:u.mem_write(note+offset,bytes([n]))
    for offset,n in [(288,kind),(300,flags),(396,count),(328,side),(316,0),(320,0)]:put(chart+offset,n)
    u.mem_write(remote+12865,b'\x01')
    total=0
    for i,d in enumerate(durations):
        total+=d;put(extras+28*i+8,total//30*500);put(extras+28*i+12,0)
    put(stage+120,0);u.mem_write(stage+177,bytes([disabled]));u.mem_write(manager+28,b'\x00')
    try:call(0x1015f8c0,ecx=stage,args=(note,grade,reason,0))
    except Exception as error:raise AssertionError((iteration,kind,grade,reason,hex(u.reg_read(UC_X86_REG_EIP)))) from error
    def read32(a):return struct.unpack('<i',u.mem_read(a,4))[0]
    out=[struct.unpack('<b',u.mem_read(note+417,1))[0],u.mem_read(note+376,1)[0],u.mem_read(note+377,1)[0],
         u.mem_read(note+411,1)[0],read32(note+420),read32(note+412),read32(note+312),read32(note+364),
         read32(note+360),read32(note+368),read32(note+372),u.mem_read(note+308,1)[0],u.mem_read(note+309,1)[0],
         read32(note+216),read32(stage+120),u.mem_read(manager+28,1)[0]]
    values=[kind,tick,duration,flags,cursor,ended,success,blocked,cooldown,deviation,style,segment,
            elapsed,segduration,direction,completed,held,contact,side,penalty,special,grade,reason,disabled,
            mode,diff,gauge,option,count,*durations]
    # Original gauge mode drives both post-event failure gate and completion.
    # This call already ran with default gauge=0; use 0 in the portable fixture.
    values[26]=0
    commands.append('G '+raw.hex()+' '+' '.join(map(str,values)))
    expected.append(result()+' '+' '.join(map(str,out)))
exe=ROOT/'.build'/('rbgameplay.exe' if sys.platform=='win32' else 'rbgameplay')
actual=subprocess.check_output([str(exe),'--batch'],input=('\n'.join(commands)+'\n').encode()).decode().splitlines()
assert len(actual)==len(expected),(len(actual),len(expected))
for i,(command,wanted,got) in enumerate(zip(commands,expected,actual)):
    if wanted!=got:
        parts1=wanted.split();parts2=got.split()
        differences=[(i,a,b) for i,(a,b) in enumerate(zip(bytes.fromhex(parts1[0]),bytes.fromhex(parts2[0]))) if a!=b]
        raise AssertionError((i,command,differences,parts1[1:],parts2[1:]))
report={'dll_sha256':digest,'cases':len(commands),'method':'original base-note and stage submission with presentation actors absent; original score and completion bonus execute',
    'limitations':['AFP presentation virtuals are no-op fixtures; actual graphics/audio/lifetimes not covered',
                   'linked notes absent in original differential fixtures; logical-link integration has C++ assertions',
                   'CPU/network remote-final override not included in stage differential suite',
                   'gauge-mode zero and stageModeSix suppress terminal medal presentation; medal calculation separately verified']}
(ROOT/'.build/gameplay-parity.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report))
