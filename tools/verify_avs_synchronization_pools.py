"""Compare original AVS passive pool boot helpers and arena advance, offline."""
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
    put(0x10065050,end+16) # InitializeCriticalSectionAndSpinCount import
    initialized=[]
    def hook(uc,address,size,data):
        sp=uc.reg_read(UC_X86_REG_ESP);pop=4;result=0
        if address==0x10011090:uc.mem_write(get(sp+4),bytes([get(sp+8)&255])*get(sp+12))
        elif address==end+16:initialized.append(get(sp+4));pop=12;result=1
        elif address not in [0x1000a950,0x1000a980,0x10022940,0x100228b0]:return
        uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_EIP,get(sp));uc.reg_write(UC_X86_REG_ESP,sp+pop)
    u.hook_add(UC_HOOK_CODE,hook)
    def run(address,*args):
        sp=stack+60000;put(sp,end)
        for i,value in enumerate(args):put(sp+4+i*4,value)
        u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(address,end,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==end
        return u.reg_read(UC_X86_REG_EAX)
    commands=[];expected=[];rng=random.Random(0x29410)
    cases=[[1,1,1,1],[257,2,3,4],[1,2,17,257]]+[[rng.randrange(1,129) for _ in range(4)] for _ in range(64)]
    functions=[0x1000ec00,0x10018b60,0x10019470,0x1001bf70]
    critical=[0x1007b2a4,0x1007b5e4,0x1007b600,0x1007bd40]
    sizes=[20,24,4,20]
    for counts in cases:
        run(0x10015110,arena,8);initialized.clear();cursor=start=arena+4096;out=[]
        config=arena+60000
        for i,count in enumerate(counts):u.mem_write(config+36+2*i,struct.pack('<H',count))
        for i,function in enumerate(functions):
            after=run(function,cursor,config)
            assert after>cursor
            token=get(critical[i]+24)
            out.extend([counts[i],sizes[i],token,cursor-start,after-cursor]);cursor=after
        assert initialized==critical,initialized
        out.append(cursor-start);commands.append(' '.join(map(str,counts)));expected.append(' '.join(map(str,out)))
    actual=subprocess.check_output([str(a.executable),'--batch'],input='\n'.join(commands)+'\n',text=True).splitlines()
    assert len(actual)==len(expected)
    for command,x,y in zip(commands,actual,expected):assert x==y,(command,x,y)
    a.report.write_text(json.dumps({'status':'passed','cases':len(cases),'original_boot_stages':len(cases)*4,'dll_sha256':digest,
        'functions':['1000EC00','10018B60','10019470','1001BF70','10039D20','1000F900','1000F6A0','100148F0'],
        'limits':'Original pool boot, descriptor allocation, arena layout and advance instructions execute. Windows critical-section initialization, descriptor locking, memset and logging are hooks. Native failure unwind is covered separately. This validates allocated pools only, not condition/event/semaphore object operations.'},indent=2)+'\n',encoding='utf-8')
    print('PASS',len(cases),'original synchronization pool boot cases')
if __name__=='__main__':main()
