"""Original isolated route instructions and the original D3DX scalar normalization.
No process, OS API, transport or resource conversion is invoked.
"""
from original_score_fixture import *
from unicorn.x86_const import UC_X86_REG_FPCW
u.reg_write(UC_X86_REG_FPCW,0x027f)

d3d_data=(ROOT.parent/'modules/d3dx9_41.dll').read_bytes()
d3d_digest=hashlib.sha256(d3d_data).hexdigest()
assert d3d_digest=='3866ed143c8e32ba08b0de985575bccefb1498176cbfc6afb05425db5a5069b0'
d3d=pefile.PE(data=d3d_data);d3d.relocate_image(0x20000000)
u.mem_map(0x20000000,(d3d.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
u.mem_write(0x20000000,d3d.get_memory_mapped_image())
# Only imported dependency used by scalar fallback: CRT _CIsqrt(ST0).
# Bind it to x87 fsqrt; no CRT initialization, OS API or library entrypoint.
u.mem_write(end+0x500,b'\xd9\xfa\xc3');put(0x20001330,end+0x500)
# Select the library's real scalar fallback, not its Windows CPU-dispatch init.
normalize_va=0x10024324
u.mem_write(normalize_va,b'\xe9'+word(0x201d54a4-normalize_va-5))
u.ctl_remove_cache(normalize_va,normalize_va+16)
call(0x100a4c90)
commands=[];expected=[];counts={}
def add(category,command,result):
    counts[category]=counts.get(category,0)+1
    commands.append(command);expected.append(result)
route=scratch+0x8000;source=scratch+0x8400
for case in range(3000):
    x=rng.randint(1,767);y=rng.randint(117,1185)
    mids=[rng.randint(0,34) if i<case%4 else -1 for i in range(3)]
    dest=rng.randint(0,34);side=case%2;kind=case%5;suppress=case%3==0;wide=case%3-1
    call(0x100a3fe0,eax=route)
    u.mem_write(source,struct.pack('<6i',x,y,*mids,dest))
    try:call(0x100a46c0,eax=source,args=(route,side,kind,int(suppress),wide))
    except Exception:
        print('failed pc',hex(u.reg_read(UC_X86_REG_EIP)),'case',case);raise
    raw=bytes(u.mem_read(route,124))
    vals=list(struct.unpack('<14i',raw[:56]));vals.extend((struct.unpack_from('<I',raw,56)[0],raw[60],struct.unpack_from('<I',raw,120)[0]))
    vals.extend(struct.unpack_from('<7I',raw,64));vals.extend(struct.unpack_from('<7I',raw,92))
    add('route_with_original_d3dx_scalar',f'R {x} {y} '+' '.join(map(str,mids))+f' {dest} {side} {kind} {int(suppress)} {wide}',' '.join(map(str,vals)))
for case in range(2000):
    x=rng.choice((-20,0,1,767,768,800,rng.randint(0,768)));y=rng.randint(80,1222)
    klass=case%3;side=case%2;dest=case%35;direction=(case//2)%2
    u.mem_write(route,struct.pack('<6i',0,0,-1,-1,-1,-1))
    u.mem_write(source,bytes(44))
    for o,v in [(0,x),(4,y),(8,klass),(16,side),(32,dest),(36,direction)]:put(source+o,v)
    call(0x100a3270,ebx=route,esi=source)
    vals=struct.unpack('<6i',u.mem_read(route,24))
    add('standard_seed',f'S {x} {y} {klass} {side} {dest} {direction}',' '.join(map(str,vals)))
for case in range(600):
    variant=case%5;side=case%2;dest=case%35;direction=case%3;kind=case%15-1;parameter=case*3;counter=case%9
    x,y=rng.randint(1,768),rng.randint(80,1222)
    u.mem_write(chart,bytes(448));u.mem_write(note,bytes(448));u.mem_write(source,bytes(44));u.mem_write(route,bytes(400))
    for off,v in [(284,variant),(328,side),(360,dest),(364,direction)]:put(chart+off,v)
    put(note+212,route);put(route+124,x);put(route+128,y);put(0x12479018,counter)
    call(0x100a3380,eax=chart,ebx=kind,esi=source,args=(note,parameter))
    vals=struct.unpack('<11i',u.mem_read(source,44))
    add('chart_config',f'C {variant} {side} {dest} {direction} {kind} {parameter} {counter} {x} {y}',' '.join(map(str,vals)))
for case in range(3000):
    initial=[rng.randint(1,767),rng.randint(117,1185),-1,-1,-1,rng.randint(9,15)]
    variant=case%3;side=(case//3)%2;dest=(25+case%3) if variant else 9+case%7;direction=(case//6)%2;which=(case//12)%4-1
    x,y=rng.randint(1,767),rng.randint(117,1185)
    u.mem_write(route,struct.pack('<6i',*initial));u.mem_write(source,bytes(44))
    for off,v in [(12,variant),(16,side),(32,dest),(36,direction),(40,which)]:put(source+off,v)
    call(0x100a3210,eax=x,edx=y,ecx=route,args=(source,))
    vals=list(struct.unpack('<6i',u.mem_read(route,24)))+[struct.unpack('<i',u.mem_read(source+36,4))[0]]
    add('redirect', 'J '+' '.join(map(str,initial))+f' {variant} {side} {dest} {direction} {which} {x} {y}', ' '.join(map(str,vals)))
exe=ROOT/'.build/rbroute.exe'
actual=subprocess.run([str(exe),'--batch'],input='\n'.join(commands)+'\n',text=True,capture_output=True,check=True).stdout.splitlines()
assert len(actual)==len(expected)
for i,(a,b) in enumerate(zip(actual,expected)):
    assert a.strip()==b.strip(),f'{i}: {commands[i]}\nport {a}\noriginal {b}'
report={'dll_sha256':digest,'d3dx_sha256':d3d_digest,'x87_control_word':'0x027f','passed':True,'cases':counts,'limitations':['D3DX scalar fallback executes original instructions; runtime CPU-specific D3DX dispatch alternatives have not been validated.','CRT _CIsqrt is bound to x87 fsqrt. Stage call order, linked-note actor state, JR activation and chart-to-route random generation are separate.']}
(ROOT/'.build/route-parity.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
