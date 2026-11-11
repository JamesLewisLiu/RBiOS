"""Compare original worker admission/recycling instructions with real native workers."""
import argparse, hashlib, json, random, struct, subprocess, sys
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['dll','dependencies','executable','tasks-executable','report']:p.add_argument('--'+name,type=Path,required=True)
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
    def short(address):return struct.unpack('<H',u.mem_read(address,2))[0]
    signatures={'InitializeCriticalSectionAndSpinCount':2,'DeleteCriticalSection':1,'EnterCriticalSection':1,'LeaveCriticalSection':1,
        'CreateMutexA':3,'CreateSemaphoreA':4,'WaitForSingleObject':2,'ReleaseMutex':1,'ReleaseSemaphore':3,'CloseHandle':1,
        'SetThreadPriority':2,'SetThreadPriorityBoost':2}
    imports={}
    for table in pe.DIRECTORY_ENTRY_IMPORT:
        for symbol in table.imports:
            name=symbol.name.decode() if symbol.name else ''
            if name in signatures:
                address=end+16+len(imports)*16;imports[address]=name;put(symbol.address,address)
    action=end+512
    export_failure=False;task_free=True
    def hook(uc,address,size,data):
        nonlocal task_free
        sp=uc.reg_read(UC_X86_REG_ESP);result=0;pop=0
        if address==0x10011090:uc.mem_write(get(sp+4),bytes([get(sp+8)&255])*get(sp+12))
        elif address==0x100396e0:
            target,entry,worker,stack_size,priority,affinity=[get(sp+4+4*i) for i in range(6)]
            assert entry==0x10041660
            put(target,900);put(target+4,0)
        elif export_failure and address==0x10039ce0:
            result=arena+50000 if task_free else 0;task_free=False
        elif export_failure and address==0x10039cb0:task_free=True
        elif export_failure and address in [0x100148f0,0x1000a980]:result=0
        elif address==action:result=42
        elif address in imports:
            pop=4*signatures[imports[address]];result=0 if imports[address]=='WaitForSingleObject' else 1
        else:return
        uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_EIP,get(sp));uc.reg_write(UC_X86_REG_ESP,sp+4+pop)
    u.hook_add(UC_HOOK_CODE,hook)
    def run(address,*args):
        sp=stack+60000;put(sp,end)
        for i,value in enumerate(args):put(sp+4+4*i,value)
        u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(address,end,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==end,hex(u.reg_read(UC_X86_REG_EIP))
        return u.reg_read(UC_X86_REG_EAX)
    def status(worker):
        affinity=get(worker+64);affinity=affinity if affinity<0x80000000 else affinity-0x100000000
        return f'{(worker-arena-316)//72} {short(worker+68)} {get(worker+60)} {get(worker+56)} {short(worker+70)} {affinity}'
    inputs=[];expected=[];commands=0;cases=0;randomizer=random.Random(17391)
    for count,initial in [(1,1),(3,0),(3,2),(4,4)]:
        for expiry in [0,1,2,4]:
            for mask in [0,3,0x80000001]:
                cases+=1;u.mem_write(arena,bytes(65536));configuration=arena+60000
                for i,value in enumerate([mask,0,count,initial,expiry,65536,2]):put(configuration+4*i,value)
                assert run(0x10041e40,arena,72*count+331,configuration)==arena
                inputs.append(f'I {count} {initial} {expiry} 65536 {mask}');expected.append('I')
                checked=[]
                for step in range(100):
                    if checked and (randomizer.randrange(3)==0 or len(checked)==count):
                        worker=checked.pop(randomizer.randrange(len(checked)));slot=(worker-arena-316)//72
                        task=arena+50000
                        run(0x100411b0,task);run(0x10041160,task,0x20004,action,0,0)
                        run(0x100414c0,worker,task);assert run(0x10041130,task)==42
                        u.reg_write(UC_X86_REG_EAX,worker);run(0x10041750)
                        if short(worker+68)==0:
                            u.reg_write(UC_X86_REG_EAX,worker);run(0x100416a0)
                        inputs.append(f'T {slot}');expected.append(status(worker))
                    else:
                        requested=randomizer.choice([0,4096,65536,131072]);priority=randomizer.randrange(5);usage=randomizer.choice([0,1,2,5])
                        worker=run(0x10041b00,arena,requested,priority,usage)
                        inputs.append(f'C {requested} {priority} {usage}');expected.append(status(worker) if worker else 'none')
                        if worker:checked.append(worker)
                    commands+=1
    actual=subprocess.check_output([str(a.executable),'--admission'],input='\n'.join(inputs)+'\n',text=True,timeout=120).splitlines()
    assert actual==expected,next(((i,inputs[i],x,y) for i,(x,y) in enumerate(zip(actual,expected)) if x!=y),(len(actual),len(expected)))
    # Execute export 091 itself with exhausted descriptors. The pool/control and
    # worker branches remain real instructions, including the stranded worker.
    u.mem_write(arena,bytes(65536));configuration=arena+60000
    for i,value in enumerate([3,0,1,1,0,65536,2]):put(configuration+4*i,value)
    assert run(0x10041e40,arena,403,configuration)==arena
    put(0x1007bb60,arena);put(0x1007bc18,4096);put(0x1007bc1c,131072)
    export_failure=True;failure_expected=[]
    for i in range(2):
        result=run(0x1001a7d0,action,0,4096,2)
        failure_expected.append(f'{result} {int(task_free)} {short(arena+316+68)}')
    failure_actual=subprocess.check_output([str(a.tasks_executable),'--failure'],text=True,timeout=30).splitlines()
    assert failure_actual==failure_expected,(failure_actual,failure_expected)
    a.report.write_text(json.dumps({'status':'passed','pool_cases':cases,'commands':commands,'dll_sha256':digest,
        'functions':['10041E40','10041B00','10041D80','10041470','10041500','10041560','10041580','10041A90','10041AC0','1005BFA0','1005BFD0','1005C1A0'],
        'export_091_failure_cases':len(failure_expected),
        'limits':'OS creation, synchronization and memset are hooked. Admission, lists, retirement, affinity and task states execute original instructions. Export 091 exhaustion additionally hooks resource allocation/release, descriptor exhaustion and logging. Native comparison uses real worker threads and a real finite resource/descriptor pool.'},indent=2)+'\n',encoding='utf-8')
    print('PASS',cases,'original pools and',commands,'admission/retirement/recycling commands')
if __name__=='__main__':main()
