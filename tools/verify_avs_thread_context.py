"""Compare original main-thread registration decisions; OS APIs are offline hooks."""
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
    signatures={'CreateMutexA':3,'CreateSemaphoreA':4,'WaitForSingleObject':2,'ReleaseMutex':1,
        'ReleaseSemaphore':3,'GetCurrentProcess':0,'GetCurrentThread':0,'DuplicateHandle':7,'TlsSetValue':2,'SetThreadPriority':2}
    imports={}
    for table in pe.DIRECTORY_ENTRY_IMPORT:
        for symbol in table.imports:
            name=symbol.name.decode() if symbol.name else ''
            if name in signatures:
                address=end+16+len(imports)*16;imports[address]=name;put(symbol.address,address)
    priorities=[];tls=[];next_handle=100
    def hook(uc,address,size,data):
        nonlocal next_handle
        sp=uc.reg_read(UC_X86_REG_ESP);result=0;pop=0
        if address==0x10011090:uc.mem_write(get(sp+4),bytes([get(sp+8)&255])*get(sp+12))
        elif address in [0x1000a950,0x1000a980]:pass
        elif address in imports:
            name=imports[address];pop=4*signatures[name];result=1
            if name.startswith('Create'):
                next_handle+=1;result=next_handle
            elif name=='WaitForSingleObject':result=0
            elif name=='DuplicateHandle':put(get(sp+16),900)
            elif name=='TlsSetValue':tls.append(get(sp+8))
            elif name=='SetThreadPriority':priorities.append(struct.unpack('<i',struct.pack('<I',get(sp+8)))[0])
        else:return
        uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_EIP,get(sp));uc.reg_write(UC_X86_REG_ESP,sp+4+pop)
    u.hook_add(UC_HOOK_CODE,hook)
    def run(address,*args):
        sp=stack+60000;put(sp,end)
        for i,value in enumerate(args):put(sp+4+i*4,value)
        u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(address,end,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==end
    expected=[]
    for priority in range(5):
        run(0x10015110,arena,8);priorities.clear();tls.clear()
        run(0x1001aa40,priority)
        assert get(0x1007bba8)==0x1007bbd0 and get(0x1007bbd0+12)==0x1007bba8
        assert get(0x1007bba8+36)==0
        assert struct.unpack('<H',u.mem_read(0x1007bbd0+68,2))[0]==3
        stored=struct.unpack('<H',u.mem_read(0x1007bbd0+70,2))[0]
        expected.append(f'{get(0x1007bba8+32)} {stored} {len(priorities)} {priorities[0] if priorities else 0} {int(tls==[0x1007bbd0])}')
    actual=subprocess.check_output([str(a.executable),'--start'],text=True).splitlines()
    assert actual==expected,(actual,expected)
    a.report.write_text(json.dumps({'status':'passed','priorities':5,'dll_sha256':digest,
        'functions':['1001AA40','100411B0','100417C0','100414C0','10039690','10039650','10041160','100397F0'],
        'limits':'Native Win32 OS operations hooked; real descriptor allocation and initialization instructions execute. Checks successful registration/TLS/priority choices only; worker bootstrap, OS priority failure, and original thread completion wait not exercised.'},indent=2)+'\n',encoding='utf-8')
    print('PASS original main-thread registration for all five priorities')
if __name__=='__main__':main()
