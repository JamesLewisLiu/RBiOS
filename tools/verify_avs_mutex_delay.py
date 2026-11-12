"""Verify original mutex delay assignment and post-acquisition Sleep calls."""
import argparse,hashlib,json,struct,subprocess,sys
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
    for address in [arena,stack,end]:u.mem_map(address,65536)
    def put(address,value):u.mem_write(address,struct.pack('<I',value&0xffffffff))
    def get(address):return struct.unpack('<I',u.mem_read(address,4))[0]
    stubs={b'TlsGetValue':end+16,b'Sleep':end+32,b'SwitchToThread':end+48}
    for module in pe.DIRECTORY_ENTRY_IMPORT:
        for imp in module.imports:
            if imp.name in stubs:put(imp.address,stubs[imp.name])
    trace=[]
    def hook(uc,address,size,data):
        sp=uc.reg_read(UC_X86_REG_ESP);pop=4;result=0
        if address==0x10011090:uc.mem_write(get(sp+4),bytes([get(sp+8)&255])*get(sp+12))
        elif address==0x10039d20:result=get(sp+8)+128
        elif address==0x10039930:trace.append(('lock',get(sp+8)))
        elif address==end+16:pop=8;result=arena+8192
        elif address==end+32:pop=8;trace.append(('sleep',get(sp+4)))
        elif address==end+48:trace.append(('yield',0))
        elif address not in [0x1000a950,0x1000a980]:return
        uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_EIP,get(sp));uc.reg_write(UC_X86_REG_ESP,sp+pop)
    u.hook_add(UC_HOOK_CODE,hook)
    def run(address,*args):
        sp=stack+60000;put(sp,end)
        for i,value in enumerate(args):put(sp+4+i*4,value)
        u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(address,end,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==end
        return u.reg_read(UC_X86_REG_EAX)
    commands=[];expected=[]
    for delay in [0,1,17,1000,65535]:
        for count in [1,2,3]:
            run(0x10015110,arena,8);token=run(0x100148f0,3,0,arena+4096)
            config=arena+60000;u.mem_write(config+34,struct.pack('<H',1));u.mem_write(config+46,struct.pack('<H',delay))
            run(0x10022b80,arena+12000,config);trace.clear()
            for _ in range(count):run(0x10022940,token)
            wanted=[('lock',arena+8192)]+([('sleep',delay)] if delay else [])
            assert trace==wanted*count,trace
            slept=[value for name,value in trace if name=='sleep']
            commands.append(f'{delay} {count}');expected.append(' '.join(map(str,[len(slept)]+slept)))
            trace.clear();run(0x10022940,0);assert not trace
    actual=subprocess.check_output([str(a.executable),'--delay-batch'],input='\n'.join(commands)+'\n',text=True).splitlines()
    assert actual==expected,(actual,expected)
    a.report.write_text(json.dumps({'status':'passed','cases':len(commands),'dll_sha256':digest,
        'functions':['10022B80','10022940','10039630','100146C0'],
        'limits':'Original configuration assignment, registered-thread lock wrapper, delay guard, and Sleep-vs-yield branch execute. Pool creation, TLS lookup, underlying lock primitive, Sleep/yield, memset and logging are hooks. Native tests separately validate retained ownership, recursive calls, invalid thread/handle rejection and injected sleep failure unwind. OS sleep timing is not measured.'},indent=2)+'\n',encoding='utf-8')
    print('PASS',len(commands),'original mutex delay cases')
if __name__=='__main__':main()
