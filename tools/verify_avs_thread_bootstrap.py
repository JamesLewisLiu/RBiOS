"""Original AVS thread bootstrap arithmetic and affinity oracle; no worker execution."""
import argparse,hashlib,json,random,struct,subprocess,sys
from pathlib import Path
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['dll','dependencies','executable','report']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args()
    if a.report.exists():raise ValueError('Report already exists')
    sys.path.insert(0,str(a.dependencies))
    import pefile
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
    raw=a.dll.read_bytes();digest=hashlib.sha256(raw).hexdigest()
    assert digest=='d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
    arena,stack,end=0x7000000,0x8000000,0x9000000
    for address in [arena,stack,end]:u.mem_map(address,65536)
    def put(address,value):u.mem_write(address,struct.pack('<I',value&0xffffffff))
    def get(address):return struct.unpack('<I',u.mem_read(address,4))[0]
    def string(address):
        data=bytearray()
        while True:
            b=bytes(u.mem_read(address,1));address+=1
            if b==b'\0':return data.decode('ascii')
            data+=b
    cpu=0;warnings=[];captured=[];task_bytes=0;fatal=False
    def hook(uc,address,size,data):
        nonlocal task_bytes,fatal
        sp=uc.reg_read(UC_X86_REG_ESP);result=0;pop=0
        if address==0x10039570:result=cpu
        elif address in [0x10041090,0x100395f0]:result=0
        elif address==0x10039d20:
            assert get(sp+12)==40
            task_bytes=(get(sp+16)*48+51)&~15;result=get(sp+8)+task_bytes
        elif address==0x10041e40:
            captured[:]=[get(get(sp+12)+i*4) for i in range(7)]+[get(sp+8)]
            result=get(sp+4)
        elif address==0x1000a980:
            message=string(get(sp+8))
            if 'pool.init_thread' in message:warnings.append(0)
            elif 'pool.expire_count' in message:warnings.append(1)
            elif 'raise sz_stack' in message:warnings.append(2)
            elif 'reduce sz_stack' in message:warnings.append(3)
            else:raise AssertionError(message)
        elif address==0x1000a780:fatal=True;uc.emu_stop();return
        elif address==end+16:pop=8;result=1
        else:return
        uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_EIP,get(sp));uc.reg_write(UC_X86_REG_ESP,sp+4+pop)
    u.hook_add(UC_HOOK_CODE,hook);put(0x10065050,end+16)
    def run(address,*args):
        sp=stack+60000;put(sp,end)
        for i,value in enumerate(args):put(sp+4+4*i,value)
        u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(address,end,count=100000)
        assert fatal or u.reg_read(UC_X86_REG_EIP)==end
        return u.reg_read(UC_X86_REG_EAX)
    inputs=[];expected=[];rng=random.Random(0x1abc0)
    for case in range(1024):
        mask=rng.getrandbits(32);cpu=mask.bit_count()
        if case<4:mask=[0,1,0x80000000,0xffffffff][case];cpu=mask.bit_count()
        count=rng.choice([0,1,2,32,808,65535]);initial=rng.randrange(65536)
        minimum=rng.choice([0,1,4096,8193,0xfffff001]);maximum=rng.choice([4096,0x200000,0x10000000,0xffffffff])
        worker=rng.choice([0,1,4096,65536,0x400000,0x7fffffff,0xffffffff]);expiry=rng.choice([0,1,2,100,0xffffffff])
        low,high=rng.getrandbits(32),rng.getrandbits(32)
        config=arena+50000;u.mem_write(config,bytes(64))
        for offset,value in [(0,low),(4,high),(8,worker),(12,expiry),(24,minimum),(28,maximum)]:put(config+offset,value)
        u.mem_write(config+16,struct.pack('<H',initial));u.mem_write(config+32,struct.pack('<H',count))
        warnings.clear();captured.clear();fatal=False
        run(0x10039530,config);result=run(0x1001abc0,arena,config)
        inputs.append(' '.join(map(str,[mask,count,initial,minimum,maximum,worker,expiry,low,high])))
        if fatal:expected.append('error');continue
        affinitylow,affinityhigh,total,init,expire,stacksz,priority,supplied=captured
        expected.append(' '.join(map(str,[cpu,get(0x1007c8d0),get(0x1007c8d8),get(0x1007bc18),get(0x1007bc1c),
            affinitylow,affinityhigh,total,init,expire,stacksz,priority,task_bytes,supplied,result-arena-task_bytes]+warnings)))
    actual=subprocess.check_output([str(a.executable),'--batch'],input='\n'.join(inputs)+'\n',text=True).splitlines()
    assert len(actual)==len(expected)
    for command,x,y in zip(inputs,actual,expected):assert x==y,(command,x,y)
    sequences=[];affinity_expected=[]
    for mask,count in [(0,4),(1,65540),(3,131080),(0xffffffffffffffff,256)]+[(rng.getrandbits(64),128) for _ in range(32)]:
        fatal=False;u.mem_write(arena,bytes(260));run(0x1005c1d0,arena,mask&0xffffffff,mask>>32)
        values=[]
        for _ in range(count):
            value=run(0x1005c1a0,arena);values.append(value if value<0x80000000 else value-0x100000000)
        sequences.append(f'{mask} {count}');affinity_expected.append(' '.join(map(str,values)))
    actual=subprocess.check_output([str(a.executable),'--affinity'],input='\n'.join(sequences)+'\n',text=True).splitlines()
    assert actual==affinity_expected
    a.report.write_text(json.dumps({'status':'passed','bootstrap_cases':len(inputs),'affinity_sequences':len(sequences),
        'dll_sha256':digest,'limits':'Bootstrap policy and exact affinity instructions only. Platform initialization, task allocation and worker construction are captured boundaries, not successful native bootstrap claims.'},indent=2)+'\n',encoding='utf-8')
    print('PASS',len(inputs),'original thread bootstrap cases and',len(sequences),'affinity sequences')
if __name__=='__main__':main()
