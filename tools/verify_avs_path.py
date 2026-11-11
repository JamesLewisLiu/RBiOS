"""Run the original AVS path normalizer unchanged against the native port."""
import argparse, hashlib, json, random, struct, subprocess, sys
from pathlib import Path
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('game-root','dependencies','executable','report'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args()
    if a.report.exists():raise ValueError('Report already exists')
    for name in ('modules','data'):
        if a.report.resolve().is_relative_to((a.game_root/name).resolve()):raise ValueError('Report must be outside assets')
    sys.path.insert(0,str(a.dependencies))
    import pefile
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
    from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EDX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
    raw=(a.game_root/'modules/libavs-win32.dll').read_bytes();digest=hashlib.sha256(raw).hexdigest()
    assert digest=='d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
    mem,stack,end=0x7000000,0x8000000,0x9000000
    for addr in (mem,stack,end):u.mem_map(addr,65536)
    cwd,path,out,hashes=mem,mem+8192,mem+16384,mem+32768
    cases=[]
    for b in range(256):
        for prefix in (b'',b'.',b'..',b'a/'):
            cases.append((128,b'/cwd',prefix+bytes([b])+b'tail'))
    for count in (0,1,63,64,65):
        for ending in (b'',b'/',b'/..',b'/../x',b'/.'):
            cases.append((256,b'',b'/x'*count+ending))
    samples=[b'',b'.',b'..',b'...',b'.abc',b'..abc',b'/a/b/../../..',b'a//b/./c',b'/absolute',b'a\0bad',b'///']
    for cap in (2,3,4,5,8,16,128,256):
        for c in (b'',b'/',b'/base/dir',b'/../x'):
            for value in samples:cases.append((cap,c,value))
    rng=random.Random(0x2c110)
    components=[b'',b'.',b'..',b'...',b'alpha',b'.hidden',b'..hidden',b'x'*40,b'bad\\path',b'bad:drive',b'file.ifs']
    for i in range(2000):
        c=b'/'.join(rng.choices(components,k=rng.randrange(8)))
        value=b'/'.join(rng.choices(components,k=rng.randrange(20)))
        cases.append((rng.choice((2,4,8,32,128,256)),c,value))
    rows=[];expected=[]
    for cap,c,value in cases:
        u.mem_write(cwd,c+b'\0');u.mem_write(path,value+b'\0');u.mem_write(out,b'\xcc'*1024);u.mem_write(hashes,b'\xcc'*512)
        sp=stack+60000;u.mem_write(sp,struct.pack('<6I',end,0,0,0,cap,hashes))
        for reg,v in ((UC_X86_REG_EAX,cwd),(UC_X86_REG_EDX,path),(UC_X86_REG_ECX,out),(UC_X86_REG_ESP,sp)):u.reg_write(reg,v)
        u.emu_start(0x1002c110,end,count=100000);assert u.reg_read(UC_X86_REG_EIP)==end
        code=struct.unpack('<i',struct.pack('<I',u.reg_read(UC_X86_REG_EAX)))[0]
        answer=f'{code} -'
        if code>=0:
            length=code&65535;depth=code>>16
            data=bytes(u.mem_read(out,length+1));assert data[-1]==0
            hs=struct.unpack('<'+'I'*(depth+1),u.mem_read(hashes,4*(depth+1)))
            answer=f'{code} {data[:-1].hex()}'+''.join(f' {h}' for h in hs)
        expected.append(answer);rows.append(f'{cap} {c.hex() or "-"} {value.hex() or "-"}')
    actual=subprocess.check_output([str(a.executable),'--batch'],input='\n'.join(rows)+'\n',text=True).splitlines()
    assert len(actual)==len(expected)
    for i,(got,want) in enumerate(zip(actual,expected)):
        assert got==want,f'case {i} {cases[i]!r}\nnative {got}\noriginal {want}'
    a.report.parent.mkdir(parents=True,exist_ok=True)
    a.report.write_text(json.dumps({'status':'passed','cases':len(cases),'address':'0x1002C110','dll_sha256':digest,
        'compared':'Signed status; successful normalized path, component count, byte length and all prefix hashes',
        'limits':'Original function executes without hooks or substituted calls. Error output buffers are not exposed or compared. No actual filesystem I/O or mount occurs.'},indent=2)+'\n')
    print(f'PASS {len(cases)} unchanged original AVS path instruction comparisons')
if __name__=='__main__':main()
