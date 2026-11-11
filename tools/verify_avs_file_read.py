"""Compare original AVS vector-read instructions with the real host file adapter."""
import argparse,hashlib,json,random,struct,subprocess,sys
from pathlib import Path
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('game-root','dependencies','executable','output-directory','report'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args()
    if a.report.exists():raise ValueError('Report already exists')
    for path in (a.output_directory,a.report):
        for name in ('modules','data'):
            if path.resolve().is_relative_to((a.game_root/name).resolve()):raise ValueError('Outputs must be outside assets')
    sys.path.insert(0,str(a.dependencies))
    import pefile
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
    raw=(a.game_root/'modules/libavs-win32.dll').read_bytes();digest=hashlib.sha256(raw).hexdigest()
    assert digest=='d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
    mem,stack,end,read_api=0x7000000,0x8000000,0x9000000,0x9001000
    for address in (mem,stack,end):u.mem_map(address,65536)
    u.mem_write(0x10065190,struct.pack('<I',read_api))
    state={}
    def hook(u,address,size,_):
        if address!=read_api:return
        sp=u.reg_read(UC_X86_REG_ESP)
        ret,handle,buffer,count,received,overlap=struct.unpack('<6I',u.mem_read(sp,24))
        assert handle==123 and overlap==0
        data=state['data'][state['position']:state['position']+count]
        if data:u.mem_write(buffer,data)
        state['position']+=len(data);u.mem_write(received,struct.pack('<I',len(data)))
        u.reg_write(UC_X86_REG_EAX,1);u.reg_write(UC_X86_REG_ESP,sp+24);u.reg_write(UC_X86_REG_EIP,ret)
    u.hook_add(UC_HOOK_CODE,hook)
    rng=random.Random(0x2a130);rows=[];expected=[]
    for i in range(400):
        length=rng.randrange(256);offset=rng.randrange(length+20);sizes=[rng.randrange(65) for _ in range(rng.randrange(9))]
        state.update(data=bytes(j%251 for j in range(length)),position=offset)
        u.mem_write(mem,struct.pack('<I',123))
        for j,size in enumerate(sizes):
            u.mem_write(mem+256+j*8,struct.pack('<II',mem+4096+j*512,size))
            u.mem_write(mem+4096+j*512,b'\xcc'*max(size,1))
        sp=stack+60000;u.mem_write(sp,struct.pack('<4I',end,mem,mem+256,len(sizes)));u.reg_write(UC_X86_REG_ESP,sp)
        u.emu_start(0x1002a130,end,count=10000);assert u.reg_read(UC_X86_REG_EIP)==end
        result=u.reg_read(UC_X86_REG_EAX)
        data=[bytes(u.mem_read(mem+4096+j*512,size)).hex() if size else '-' for j,size in enumerate(sizes)]
        expected.append(f"{result} {state['position']}"+''.join(' '+v for v in data))
        rows.append(f'{length} {offset} {len(sizes)}'+''.join(' '+str(v) for v in sizes))
    actual=subprocess.check_output([str(a.executable),'--batch',str(a.output_directory)],input='\n'.join(rows)+'\n',text=True).splitlines()
    assert len(actual)==len(expected)
    for i,(got,want) in enumerate(zip(actual,expected)):assert got==want,f'{i}: {got} != {want}'
    a.report.write_text(json.dumps({'status':'passed','cases':len(rows),'address':'0x1002A130','dll_sha256':digest,
        'compared':'Original sequential ReadFile callback flow versus real host files: return count, position and complete buffer contents including untouched tails',
        'limits':'Original ReadFile is replaced by an in-memory byte source. Native adapter opens actual generated fixtures. Error mapping and other callbacks are not instruction-oracle validated by this script.'},indent=2)+'\n')
    print(f'PASS {len(rows)} original AVS readv versus real host file comparisons')
if __name__=='__main__':main()
