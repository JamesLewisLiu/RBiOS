"""Compare original default-fs mount/parser instructions with native option recovery."""
import argparse,hashlib,json,random,struct,subprocess,sys
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
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
    raw=(a.game_root/'modules/libavs-win32.dll').read_bytes();digest=hashlib.sha256(raw).hexdigest()
    assert digest=='d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
    mem,stack,end=0x7000000,0x8000000,0x9000000
    for address in (mem,stack,end):u.mem_map(address,65536)
    def hook(u,address,size,_):
        if address not in (0x1002ae30,0x1002adf0):return
        sp=u.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',u.mem_read(sp,8))
        if address==0x1002ae30:assert arg==272
        u.reg_write(UC_X86_REG_EAX,mem+32768 if address==0x1002ae30 else 0)
        u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    u.hook_add(UC_HOOK_CODE,hook)
    cases=[b'',b'posix=0',b'posix=1',b'vf=0',b'sz_block=512,nr_block=32,nr_mirror=4']
    values=['','0','1','2','-1','+1','0x100','010','08','0x','0XFF','2147483648','4294967296','-2147483649','0000',' 1','1 ','wrong','"1"','" 1"','"1 "','""','1\\,2','" "','"+"','"-"','"0x"','0xg','0x100000000','-0x100000000','040000000000','18446744073709551616']
    for key in ('posix','vf','unknown'):
        for value in values:
            for prefix in ('','posix=1,','vf=0,'):
                cases.append((prefix+key+'='+value).encode())
    for c in range(256):cases.append(b'posix=1'+bytes([c])+b'vf=0')
    for n in (250,255,256,265,266,270,271,272,273,300):cases.append(b'unknown='+b'x'*n)
    rng=random.Random(0x288c0)
    parts=['posix=0','posix=1','vf=0','vf=1','unknown=value','sz_block=512','posix','vf','"posix"=1','posix="0x1"','\\','',',',' ', '\t']
    for i in range(700):cases.append(','.join(rng.choices(parts,k=rng.randrange(1,8))).encode())
    rows=[];expected=[]
    for text in cases:
        u.mem_write(mem,b'\xa5\xa5');u.mem_write(mem+1024,text+b'\0')
        sp=stack+60000;u.mem_write(sp,struct.pack('<5I',end,mem,0,0,mem+1024));u.reg_write(UC_X86_REG_ESP,sp)
        u.emu_start(0x1002a960,end,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==end
        status=struct.unpack('<i',struct.pack('<I',u.reg_read(UC_X86_REG_EAX)))[0]
        options=bytes(u.mem_read(mem,2));expected.append(f'{status} {options[0]} {options[1]}');rows.append(text.hex() or '-')
    actual=subprocess.check_output([str(a.executable),'--batch'],input='\n'.join(rows)+'\n',text=True).splitlines()
    assert len(actual)==len(expected)
    mismatches=[f'{i} {cases[i]!r}: {got} != {want}' for i,(got,want) in enumerate(zip(actual,expected)) if got!=want]
    assert not mismatches,'\n'.join(mismatches[:30])
    a.report.write_text(json.dumps({'status':'passed','cases':len(rows),'dll_sha256':digest,
        'addresses':['0x1002A960','0x10028810','0x100288C0','0x10028BE0','0x10028C30','0x10028E30'],
        'compared':'Final mount return and both option bytes, including partial callback effects before failure',
        'limits':'Only scratch allocation/free intercepted. Original state machine, tokenizer, numeric converter, lookup and mount callbacks execute unchanged. No mount or host file operation occurs.'},indent=2)+'\n')
    print(f'PASS {len(rows)} original default-fs mount option comparisons')
if __name__=='__main__':main()
