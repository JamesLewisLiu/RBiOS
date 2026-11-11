"""Compare native workers with original pool initialization and task-loop leaf instructions."""
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
        'CreateMutexA':3,'CreateSemaphoreA':4,'WaitForSingleObject':2,'ReleaseMutex':1,'ReleaseSemaphore':3,'CloseHandle':1}
    imports={}
    for table in pe.DIRECTORY_ENTRY_IMPORT:
        for symbol in table.imports:
            name=symbol.name.decode() if symbol.name else ''
            if name in signatures:
                address=end+16+len(imports)*16;imports[address]=name;put(symbol.address,address)
    action=end+512;callback_base=end+544;created=[];trace=[];result_value=0
    def hook(uc,address,size,data):
        sp=uc.reg_read(UC_X86_REG_ESP);result=0;pop=0
        if address==0x10011090:uc.mem_write(get(sp+4),bytes([get(sp+8)&255])*get(sp+12))
        elif address==0x100396e0:
            target,entry,worker,stack_size,priority,affinity=[get(sp+4+4*i) for i in range(6)]
            assert entry==0x10041660 and stack_size==65536 and priority==2
            created.append(worker);put(target,900);put(target+4,0)
        elif address in [0x10041a90,0x10041ac0]:pass # Routing ownership is outside this leaf comparison.
        elif address==action:result=result_value
        elif callback_base<=address<callback_base+64 and (address-callback_base)%16==0:
            assert get(sp+4)==0x20004;trace.append(str((address-callback_base)//16))
        elif address in imports:
            pop=4*signatures[imports[address]];result=0 if imports[address]=='WaitForSingleObject' else 1
        else:return
        uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_EIP,get(sp));uc.reg_write(UC_X86_REG_ESP,sp+4+pop)
    u.hook_add(UC_HOOK_CODE,hook)
    def run(address,*args):
        sp=stack+60000;put(sp,end)
        for i,value in enumerate(args):put(sp+4+4*i,value)
        u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(address,end,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==end
        return u.reg_read(UC_X86_REG_EAX)
    cases=[(3,2,expiry,mask) for expiry in [0,1,2,3,0xffffffff] for mask in [0,3,0x80000001]]+[(1,4,2,1),(3,0,2,3)]
    inputs=[];expected=[];tasks=0
    for count,initial,expiry,mask in cases:
        created.clear();u.mem_write(arena,bytes(65536));configuration=arena+60000
        for i,value in enumerate([mask&0xffffffff,mask>>32,count,initial,expiry,65536,2]):put(configuration+4*i,value)
        assert run(0x10041e40,arena,72*count+331,configuration)==arena
        initial_line='I'
        for worker in created:
            slot=(worker-arena-316)//72
            affinity=get(worker+64);affinity=affinity if affinity<0x80000000 else affinity-0x100000000
            initial_line+=f' {slot}:{short(worker+68)}:{get(worker+60)}:{affinity}'
        expected.append(initial_line)
        for worker in created:
            slot=(worker-arena-316)//72
            for step in range(3):
                if short(worker+68)!=1:break
                task=arena+50000;callbacks=arena+51000
                run(0x100411b0,task)
                for i in range(4):put(callbacks+4*i,callback_base+16*i)
                run(0x10041160,task,0x20004,action,0,callbacks)
                run(0x100414c0,worker,task)
                result_value=42+step;trace.clear()
                assert run(0x10041130,task)==result_value
                assert get(task+36)==2 and get(task+12)==result_value
                u.reg_write(UC_X86_REG_EAX,worker);run(0x10041750)
                expected.append(f'T {slot} {result_value} {short(worker+68)} {get(worker+60)} {"".join(trace)}');tasks+=1
        expected.append('S');inputs.append(f'{count} {initial} {expiry} {mask}')
    actual=subprocess.check_output([str(a.executable),'--batch'],input='\n'.join(inputs)+'\n',text=True).splitlines()
    assert actual==expected,next(((i,x,y) for i,(x,y) in enumerate(zip(actual,expected)) if x!=y),(len(actual),len(expected)))
    a.report.write_text(json.dumps({'status':'passed','pool_cases':len(cases),'task_dispatches':tasks,'dll_sha256':digest,
        'functions':['10041E40','100417C0','10041580','100411B0','10041160','100414C0','10041130','10041230','10041750'],
        'limits':'Original OS creation/locks are hooked; native executable uses real threads. Explicit selected-slot assignment only. Idle/retired routing hooks do not claim full original admission/recycling; native shutdown is tested separately.'},indent=2)+'\n',encoding='utf-8')
    print('PASS',len(cases),'original pool creations and',tasks,'task-loop transitions')
if __name__=='__main__':main()
