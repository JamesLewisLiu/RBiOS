"""Original AVS mutex/descriptor ownership oracle; no Windows threads or network."""
import argparse, hashlib, json, random, struct, subprocess, sys
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['dll','dependencies','executable','report']: p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args()
    if a.report.exists(): raise ValueError('Report already exists')
    sys.path.insert(0,str(a.dependencies))
    import pefile
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
    raw=a.dll.read_bytes(); digest=hashlib.sha256(raw).hexdigest()
    assert digest=='d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
    arena,stack,end=0x7000000,0x8000000,0x9000000
    for address in [arena,stack,end]:u.mem_map(address,65536)
    def put(address,value):u.mem_write(address,struct.pack('<I',value&0xffffffff))
    def get(address):return struct.unpack('<I',u.mem_read(address,4))[0]
    free=[];active=set();initialized=set()
    def hook(uc,address,size,data):
        sp=uc.reg_read(UC_X86_REG_ESP);result=0
        if address==0x10011090:uc.mem_write(get(sp+4),bytes([get(sp+8)&255])*get(sp+12))
        elif address==0x10039ce0:
            if free:result=free.pop();active.add(result)
        elif address==0x100399f0:
            resource=get(sp+4);assert resource in active;initialized.add(resource)
        elif address==0x100399d0:
            resource=get(sp+4);assert resource in initialized;initialized.remove(resource)
        elif address==0x10039cb0:
            resource=get(sp+8);assert resource in active and resource not in initialized
            active.remove(resource);free.append(resource)
        elif address not in [0x1000a950,0x1000a980,0x10022940,0x100228b0]:return
        uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_EIP,get(sp));uc.reg_write(UC_X86_REG_ESP,sp+4)
    u.hook_add(UC_HOOK_CODE,hook)
    def run(address,*args):
        sp=stack+60000;put(sp,end)
        for i,value in enumerate(args):put(sp+4+i*4,value)
        u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(address,end,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==end
        return u.reg_read(UC_X86_REG_EAX)
    inputs=[];expected=[];rng=random.Random(0x22ab0)
    for descriptors,resources in [(1,1),(1,3),(8,2),(32,32)]:
        run(0x10015110,arena,descriptors)
        free[:]=[arena+32000+i*16 for i in reversed(range(resources))];active.clear();initialized.clear()
        inputs.append(f'I {descriptors} {resources}');expected.append(str(resources));live=[]
        for _ in range(500):
            if not live or rng.randrange(3):
                token=run(0x10022ab0);inputs.append('C 0 0');expected.append(f'{token} {len(free)}')
                if token:live.append(token)
            else:
                token=live.pop(rng.randrange(len(live)))
                result=run(0x10014700,token);inputs.append(f'D {token} 0');expected.append(f'{result} {len(free)}')
            assert active==initialized
    actual=subprocess.check_output([str(a.executable),'--batch'],input='\n'.join(inputs)+'\n',text=True).splitlines()
    assert len(actual)==len(expected)
    for command,x,y in zip(inputs,actual,expected):assert x==y,(command,x,y)
    a.report.write_text(json.dumps({'status':'passed','commands':len(inputs),'dll_sha256':digest,
        'functions':['10022AB0','10022B30','10015110','100148F0','10014700'],
        'limits':'Finite resource allocation, native-event init/destroy, locking and logging hooked. Original ownership and unwind instructions execute. OS event allocation failure and actual Windows scheduling not emulated; native contention tested separately.'},indent=2)+'\n',encoding='utf-8')
    print('PASS',len(inputs),'original AVS mutex ownership commands')
if __name__=='__main__':main()
