"""Compare migrated score functions with bounded execution of the original x86 bytes.

No game process, Windows APIs, or transport is invoked. A note virtual query is an
explicit boolean fixture; all score/life/gauge/rate functions execute original code.
"""
from original_score_fixture import *

commands, expected, counts = [], [], {}
def add(category, command, result):
    counts[category] = counts.get(category, 0)+1
    commands.append(command); expected.append(str(result))
for level, grade, n in itertools.product((-1,0,1,5,9,10,11,20),range(5),(0,1,100,348,349,350,351,999,3000)):
    add('life_increment',f'L {level} {grade} {n}',call(0x101546c0,eax=level,esi=grade,args=(n,)))
for index,side,reason,mode in itertools.product(range(9),range(2),(0,4,6),range(2)):
    r=fixture();load(r);put(config+8,6 if mode else 0)
    old=rng.randint(0,1000)
    call(0x10154d40,eax=reason,edx=battle,ecx=index,edi=side,args=(old,))
    add('effect',f'F {r.hex()} {index} {side} {reason} {old} {mode}',result())
for _ in range(30):
    r=fixture()
    for step in range(40):
        side=rng.randrange(2);grade=rng.randrange(5);reason=rng.randrange(9)
        kind=rng.choice([0,1,2,4]);deviation=rng.randint(-100,100)
        special=rng.randrange(2);penalty=rng.randrange(2);remote_combo=rng.randrange(2);mode=rng.randrange(2)
        load(r);put(config+8,6 if mode else 0);u.mem_write(remote+12865,bytes([remote_combo]))
        put(chart+288,kind);put(chart+328,side);put(note+412,deviation);u.mem_write(note+426,bytes([penalty]))
        u.mem_write(end+0x300,b'\xb0'+bytes([special])+b'\xc3')
        u.ctl_remove_cache(end+0x300,end+0x310)
        call(0x10155260,esi=grade,args=(battle,note,reason))
        got=result()
        add('event_sequence',f'E {r.hex()} {reason} {grade} {side} {kind} {deviation} {special} {penalty} {remote_combo} {mode}',got)
        r=bytearray.fromhex(got)
for _ in range(300):
    r=fixture()
    a=struct.unpack_from('<5i',r,172);done=sum(a);extra=rng.randint(0,a[3])
    i32(r,192,extra);i32(r,256,done+rng.randint(0,200));i32(r,260,extra)
    r[288]=rng.randrange(2);r[300]=rng.randrange(2)
    diff=rng.randrange(4);option=rng.randrange(6)
    load(r);put(config+16,diff);put(config+1080,option)
    rates=[float_bits(va) for va in (0x101642b0,0x10164340,0x101643b0)]
    values=rates+[call(0x10164450,args=(player,)),call(0x10164540,esi=player),call(0x101647c0,eax=player)]
    add('rates_and_results',f'A {r.hex()} {diff} {option}',' '.join(map(str,values)))
for _ in range(30):
    r=fixture();load(r);call(0x101548f0,eax=battle);call(0x10164160,eax=player)
    add('reset',f'R {r.hex()}',result())
for jr in (-2147483648,-2147483600,-1,0,99,100,101,500,2147483647):
    r=fixture();i32(r,272,jr);load(r);call(0x10164260,eax=player)
    add('consume_just_reflec',f'C {r.hex()}',result())
exe=ROOT/'.build'/('rbscore.exe' if sys.platform=='win32' else 'rbscore')
actual=subprocess.check_output([str(exe),'--batch'],input=('\n'.join(commands)+'\n').encode()).decode().splitlines()
assert len(actual)==len(expected),(len(actual),len(expected))
for i,(command,wanted,got) in enumerate(zip(commands,expected,actual)):
    if wanted!=got:
        if command[0] in 'EFRC':
            differences=[(n,a,b) for n,(a,b) in enumerate(zip(bytes.fromhex(wanted),bytes.fromhex(got))) if a!=b]
            raise AssertionError((i,command[:1],differences,command))
        raise AssertionError((i,command,wanted,got))
report={'dll_sha256':digest,'method':'bounded original x86 execution; full score functions; no Windows APIs or network',
        'cases':sum(counts.values()),'coverage':counts,
        'limitations':['note virtual +180 is supplied as a boolean fixture; note subtype implementation not validated',
                      'stage configuration is explicit fixture data; full stage initializer not recovered',
                      'does not validate touch-to-judgment dispatch, render or audio integration',
                      'nonfinite gauges excluded; adapter rejects zero denominators and invalid grade/side']}
(ROOT/'.build/score-parity.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report))
