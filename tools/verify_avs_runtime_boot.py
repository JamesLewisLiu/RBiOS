"""Compare original AVS startup order, arena cursor and descriptor identities."""
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
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EDX,UC_X86_REG_ECX
    raw=a.dll.read_bytes();digest=hashlib.sha256(raw).hexdigest()
    assert digest=='d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
    arena,stack,end=0x7000000,0x8000000,0x9000000
    u.mem_map(arena,0x800000)
    for address in [stack,end]:u.mem_map(address,65536)
    def put(address,value):u.mem_write(address,struct.pack('<I',value&0xffffffff))
    def get(address):return struct.unpack('<I',u.mem_read(address,4))[0]
    imports={b'InitializeCriticalSectionAndSpinCount':(2,1),b'CreateMutexA':(3,1),b'CreateSemaphoreA':(4,2),b'CloseHandle':(1,1),b'EnterCriticalSection':(1,0),b'LeaveCriticalSection':(1,0)}
    stubs={}
    for module in pe.DIRECTORY_ENTRY_IMPORT:
        for imp in module.imports:
            if imp.name in imports:
                target=end+16+len(stubs)*16;stubs[target]=imports[imp.name];put(imp.address,target)
    # Native main registration is a platform boundary in this oracle. Its
    # known public type-2 binding still executes original descriptor allocate.
    mainStub=end+512;mainToken=end+1024
    code=b'\x68'+struct.pack('<I',arena+0x700000)+b'\x6a\x00\x6a\x02\xe8'
    code+=struct.pack('<i',0x100148f0-(mainStub+len(code)+4))
    code+=b'\x83\xc4\x0c\xa3'+struct.pack('<I',mainToken)+b'\xc3'
    u.mem_write(mainStub,code)
    ordered=[0x10015110,0x10029410,0x10039530,0x1001abc0,0x10041e40,0x10022b80,0x1000ec00,0x10018b60,0x10019470,0x1001bf70,0x10040700,0x100150e0,0x1001aa40,0x10022cd0]
    trace=[];wrapperStarts=[];reached=False;cursor=0;workerSupply=0
    def hook(uc,address,size,data):
        nonlocal reached,cursor,workerSupply
        sp=uc.reg_read(UC_X86_REG_ESP);pop=4;result=0
        if address in ordered:trace.append(address)
        if address==0x10022cd0:cursor=get(sp+4);reached=True;uc.emu_stop();return
        if address==0x10039d20:wrapperStarts.append((get(sp+8),get(sp+12),get(sp+16)));return
        if address in stubs:
            arguments,result=stubs[address];pop+=arguments*4
        elif address==0x10011090:uc.mem_write(get(sp+4),bytes([get(sp+8)&255])*get(sp+12))
        elif address==0x10039570:result=2
        elif address==0x10041e40:workerSupply=get(sp+8);result=get(sp+4)
        elif address==0x1001aa40:uc.reg_write(UC_X86_REG_EIP,mainStub);return
        elif address==0x1000a780:raise AssertionError('Original fatal handler reached')
        elif address not in [0x1000a950,0x1000a980,0x100395f0,0x10041090,0x10041990,
                             0x100411b0,0x100417c0,0x10041580,0x100414c0,
                             0x100399f0,0x10022940,0x100228b0]:return
        uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_EIP,get(sp));uc.reg_write(UC_X86_REG_ESP,sp+pop)
    u.hook_add(UC_HOOK_CODE,hook)
    rng=random.Random(0x13bc0)
    cases=[[16,1,1,1,1,1,1,1],[17,2,3,4,5,6,7,8],[64,3,8,7,6,5,4,3],[64,4,2,3,4,5,6,7]]
    cases += [[rng.randrange(16,129),rng.randrange(1,9)]+[rng.randrange(1,33) for _ in range(6)] for _ in range(20)]
    commands=[];expected=[];residues=set();details=[]
    for values in cases:
        descriptors,threads,*counts=values
        config=arena+0x710000;u.mem_write(config,bytes(2528));put(config+96,4096);put(config+100,131072);put(config+80,65536)
        put(config+72,3);put(config+120,2)
        for offset,value in [(68,descriptors),(104,threads)]+[(106+2*i,count) for i,count in enumerate(counts)]:u.mem_write(config+offset,struct.pack('<H',value))
        trace.clear();wrapperStarts.clear();reached=False;put(mainToken,0)
        # Descriptor locking is not yet booted at the start of each independent case.
        put(0x1007b5d0,0)
        sp=stack+60000;put(sp,end)
        for register,value in [(UC_X86_REG_ESP,sp),(UC_X86_REG_EAX,config),(UC_X86_REG_EDX,arena),(UC_X86_REG_ECX,arena+0x600000)]:u.reg_write(register,value)
        try:u.emu_start(0x10013bc0,end,count=1000000)
        except Exception as error:raise RuntimeError(f'Emulation stopped at {u.reg_read(UC_X86_REG_EIP):08X}; stages {[hex(x) for x in trace]}') from error
        assert reached,hex(u.reg_read(UC_X86_REG_EIP))
        assert trace==ordered,[hex(x) for x in trace]
        assert workerSupply==threads*72+331
        assert len(wrapperStarts)==6
        # The worker cursor is deliberately not 16-byte aligned. Each wrapper
        # advances a multiple of sixteen while its array header aligns itself.
        residue=wrapperStarts[1][0]&15;residues.add(residue)
        assert residue in [4,12] and all(start&15==residue for start,_,_ in wrapperStarts[1:])
        descriptorMutex=get(0x1007b5d0);main=get(mainToken)
        result=f'{cursor-arena} {descriptorMutex} {main}'
        commands.append(' '.join(map(str,values)));expected.append(result)
        details.append({'counts':values,'arena_bytes':cursor-arena,'mutex':descriptorMutex,'main':main,'worker_cursor_mod16':residue})
    actual=subprocess.check_output([str(a.executable),'--boot-batch'],input='\n'.join(commands)+'\n',text=True).splitlines()
    assert len(actual)==len(expected),(len(actual),len(expected),actual)
    for command,x,y in zip(commands,actual,expected):assert x==y,(command,x,y)
    a.report.write_text(json.dumps({'status':'passed','cases':len(cases),'dll_sha256':digest,
        'ordered_functions':[f'{address:08X}' for address in ordered],
        'worker_cursor_mod16':sorted(residues),'cases_detail':details,
        'limits':'Original 13BC0/29410 orchestration, task and synchronization array allocators, worker cursor arithmetic, alarm record initialization and descriptor service allocation execute. Native worker start/dispatch, task synchronization initialization, native main registration, platform CPU/TLS/timing/locking imports and logging are hooks. Main type-2 token allocation still executes original descriptor instructions. Oracle stops on entry to filesystem boot. Matching order/arena/tokens does not validate full AVS or game boot.'},indent=2)+'\n',encoding='utf-8')
    print('PASS',len(cases),'original runtime boot order/arena/token cases')
if __name__=='__main__':main()
