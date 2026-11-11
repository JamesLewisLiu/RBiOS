"""Compare AVS array allocation/query instructions with native pools, offline."""
import argparse, hashlib, json, random, struct, subprocess, sys
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['dll','dependencies','executable','report']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args()
    if a.report.exists():raise ValueError('Report already exists')
    sys.path.insert(0,str(a.dependencies))
    import pefile
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
    raw=a.dll.read_bytes();digest=hashlib.sha256(raw).hexdigest()
    assert digest=='d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
    arena,stack,end=0x7000000,0x8000000,0x9000000
    for address in [arena,stack,end]:u.mem_map(address,0x10000)
    def put(address,value):u.mem_write(address,struct.pack('<I',value&0xffffffff))
    def get(address):return struct.unpack('<I',u.mem_read(address,4))[0]
    def hook(uc,address,size,data):
        sp=uc.reg_read(UC_X86_REG_ESP)
        if address==0x10011090:uc.mem_write(get(sp+4),bytes([get(sp+8)&255])*get(sp+12))
        elif address not in [0x1000a950,0x1000a980,0x10022940,0x100228b0]:return
        uc.reg_write(UC_X86_REG_EAX,0);uc.reg_write(UC_X86_REG_EIP,get(sp));uc.reg_write(UC_X86_REG_ESP,sp+4)
    u.hook_add(UC_HOOK_CODE,hook)
    def run(address,*args):
        sp=stack+60000;put(sp,end)
        for i,value in enumerate(args):put(sp+4+i*4,value)
        u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(address,end,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==end
        return u.reg_read(UC_X86_REG_EAX)
    commands=[];expected=[];rng=random.Random(0xf900);out=arena+60000
    def add(command,result):commands.append(command);expected.append(str(result))
    for size,count,alignment in [(s,c,a) for s in [1,17,60,120] for c in [1,3,17] for a in [8,32]]:
        run(0x10015110,arena,32);put(0x1007b5d0,1)
        token=run(0x1000f900,arena+4096,size,count,alignment);assert 0<token<0x80000000
        assert run(0x100146c0,token,out)
        header=get(out);stride=get(header);start=header+40
        add(f'I {size} {count} {alignment}',f'{stride} {stride-8}')
        def pointer(index):return start+index*stride if index<count else 0
        def index(ptr):return (ptr-start)//stride if start<=ptr<start+count*stride else 0xffffffff
        live=[]
        for step in range(150):
            op=rng.choice(['A','A','R','Z','Q','Q'])
            if op=='A':
                result=run(0x1000f830,token);i=index(result);add('A 0 0 0',i)
                if result:
                    live.append(i);add(f'B {i} 0 0',sum(u.mem_read(result,stride-8)))
                    value=rng.randrange(1,256);u.mem_write(result,bytes([value])*(stride-8));add(f'W {i} {value} 0',0)
            elif op=='R' and live:
                i=live.pop(rng.randrange(len(live)));add(f'R {i} 0 0',run(0x1000f6f0,token,pointer(i)))
                add(f'B {i} 0 0',sum(u.mem_read(pointer(i),stride-8)))
            elif op=='Z':
                run(0x1000f340,token,5,out,0);add('Z 0 0 0',0);live=[]
            for query in [0,1,2,rng.choice([3,4,6,7,8,9,10,11,12])]:
                i=rng.randrange(count+2);arg=pointer(i) if query in [3,6] else i
                u.mem_write(out,bytes(12));run(0x1000f340,token,query,out,arg)
                value=get(out)
                if query==2:value=' '.join(str(get(out+j*4)) for j in range(3))
                elif query>=7:value=index(value)
                elif query in [3,4]:value=u.mem_read(out,1)[0]
                add(f'Q {query} {i} 0',value)
    # Original alignment padding for the wrapper's eight-byte-aligned arrays.
    for offset in [0,4,8,12]:
        run(0x10015110,arena,32)
        for size in [1,17,60,120]:
            address=arena+4096+offset;token=run(0x1000f900,address,size,3,8)
            assert run(0x100146c0,token,out);header=get(out);stride=get(header)
            add(f'L {address} {size} 3',f'{stride} {header-address} {header+40-address} {header-address+32+stride*3}')
    actual=subprocess.check_output([str(a.executable),'--batch'],input='\n'.join(commands)+'\n',text=True).splitlines()
    assert len(actual)==len(expected),(len(actual),len(expected))
    for i,(command,x,y) in enumerate(zip(commands,actual,expected)):assert x==y,(i,command,x,y)
    a.report.write_text(json.dumps({'status':'passed','commands':len(commands),'dll_sha256':digest,
        'functions':['1000F900','1000F6A0','1000F830','1000F6F0','1000F340','1000EF00','100148F0','100146C0'],
        'limits':'Original allocator, free list, active list, descriptor validation and query instructions execute. Only memset, descriptor lock primitives and logging are hooked. Real native contention is tested separately. Fatal invalid frees and 32-bit arithmetic overflow are not executed; native code rejects these with exceptions.'},indent=2)+'\n',encoding='utf-8')
    print('PASS',len(commands),'original AVS resource pool commands')
if __name__=='__main__':main()
