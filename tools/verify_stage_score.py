"""Compare full 0x101554C0 initialization, with explicit in-memory settings/profile.

No game, OS API, saved profile or network. Original configuration defaults are
from 0x1007FFD0; varied parameters exercise the initializer independently.
"""
from original_score_fixture import *
from unicorn.x86_const import UC_X86_REG_FPCW
u.reg_write(UC_X86_REG_FPCW,0x027f)
profile=0xb000000
u.mem_map(profile,3*1024*1024)
u.mem_write(profile,b'\xff'*(3*1024*1024))
put(0x10a71070,profile)
commands=[];expected=[]
for case in range(600):
    side=rng.randrange(2);flags=rng.choice([0,1,2,3,16]);floor=rng.randrange(2)
    level=rng.choice([-1,0,1,5,9,10,11,14])
    totals=[rng.randrange(20,2000) for _ in range(2)]
    long=[rng.randrange(n) for n in totals]
    after=[rng.randrange(n-l+1) for n,l in zip(totals,long)]
    gauge=[rng.randrange(10001) for _ in range(5)] if case else [10000,10000,0,0,0]
    maximum=[rng.randrange(1,10001) for _ in range(5)] if case else [10000,10000,0,0,0]
    scales=[rng.choice([0.,.25,1.,1.5,2.]) for _ in range(4)] if case else [1.]*4
    u.mem_write(battle,bytes(172));u.mem_write(chart,bytes(512))
    u.mem_write(config,bytes(12000));put(0x10a71058,config)
    put(config+24,side);put(config+1088,floor)
    # Synthetic signed catalog level 1 permits the original explicit override.
    u.mem_write(0x101b4198+460,bytes([255 if level==-1 else 1]));put(profile+2014772,level)
    for i in range(2):
        for off,values in [(88,totals),(120,long),(112,after)]:put(chart+off+4*i,values[i])
    for i in range(5):put(config+1696+4*i,gauge[i]);put(config+1716+4*i,maximum[i])
    for i,v in enumerate(scales):u.mem_write(config+1680+4*i,struct.pack('<f',v))
    call(0x101554c0,eax=chart,edx=flags,edi=battle)
    expected.append(bytes(u.mem_read(battle,172)).hex())
    commands.append(' '.join(map(str,[side,flags,floor,level,*totals,*long,*after,*gauge,*maximum,*scales])))
exe=ROOT/'.build/rbstagescoreinitialization.exe'
actual=subprocess.check_output([str(exe),'--batch'],input=('\n'.join(commands)+'\n').encode()).decode().splitlines()
assert len(expected)==len(actual)
for i,(wanted,got) in enumerate(zip(expected,actual)):
    got=got[:344]
    if wanted!=got:
        raise AssertionError((i,[(n,a,b) for n,(a,b) in enumerate(zip(bytes.fromhex(wanted),bytes.fromhex(got))) if a!=b]))
print('PASS 600 original stage score initialization comparisons')
