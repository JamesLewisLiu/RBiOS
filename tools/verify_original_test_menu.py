"""Compare original TEST main-menu instructions; never execute hardware/network scenes."""
import argparse,hashlib,itertools,json,struct,subprocess,sys
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['game-root','dependencies','executable','report']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();sys.path.insert(0,str(a.dependencies))
    import pefile
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
    raw=(a.game_root/'modules/reflecbeat.dll').read_bytes();digest=hashlib.sha256(raw).hexdigest()
    assert digest=='ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
    pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
    arena,stack,end=0x7000000,0x8000000,0x9000000
    for address in [arena,stack,end]:u.mem_map(address,65536)
    def put(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<i',u.mem_read(at,4))[0]
    def ret(pop):
        sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,1)
        u.reg_write(UC_X86_REG_EIP,get(sp)&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+4+pop)
    dispatch=[0,0,False]
    def hook(uc,address,size,user):
        if address==end+16:ret(4)
        elif address==end+32:
            dispatch[:]=[1,get(u.reg_read(UC_X86_REG_ESP)+4),False];ret(12)
        elif address==end+48:
            dispatch[:]=[2,0,get(arena+4096+4)!=0];ret(4)
    u.hook_add(UC_HOOK_CODE,hook)
    tables=[]
    for region,address in enumerate([0x103671E8,0x103672D8]):
        rows=[];at=address-base
        while True:
            scene,label,flags=struct.unpack('<iII',pe.get_data(at,12));at+=12
            if scene==-8:break
            rows.append((scene,flags,pe.get_string_at_rva(label-base).decode('ascii') if label else ''))
        actual=subprocess.check_output([str(a.executable),'--table',str(region)],text=True).splitlines()
        assert actual==[f'{scene}|{flags}|{label}' for scene,flags,label in rows]
        tables.append((address,rows))
    inputs=[];expected=[];gate_cases=0
    for region,(table,rows) in enumerate(tables):
        for selected,pressed,repeated,blocked,mode in itertools.product(range(len(rows)),[0,0x10,0x20,0x30,1],[0,1,2,0x40,0x42,0x43],range(2),[0,1,2]):
            u.mem_write(table,pe.get_data(table-base,(len(rows)+1)*12))
            u.mem_write(arena,bytes(8192));put(0x12477D68,table);put(0x10A71078,arena+4096)
            u.mem_write(arena+4096+60,struct.pack('<H',mode));u.mem_write(0x12477CCC,bytes([blocked,0,0]));u.mem_write(0x12476DFA,b'\0')
            sp=stack+60000;put(sp,end);u.reg_write(UC_X86_REG_ESP,sp)
            u.emu_start(0x100D5780,end,count=1000);assert u.reg_read(UC_X86_REG_EIP)==end
            assert get(table+12*(len(rows)-1)+8)==(2 if blocked else 0)
            assert get(table+12*(len(rows)-2)+8)==(0 if not blocked and mode==1 else 2)
            gate_cases+=1
            put(arena,arena+1024);put(arena+1024+40,end+32);put(arena+1024+44,end+48)
            put(arena+212,arena+2048);put(arena+2048+24,end+16)
            put(arena+200,len(rows));put(arena+204,selected)
            put(0x12477D3C,pressed);put(0x12477D44,repeated);dispatch[:]=[0,0,False]
            put(sp,end);put(sp+4,0);u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,arena)
            u.emu_start(0x100D5600,end,count=1000);assert u.reg_read(UC_X86_REG_EIP)==end
            inputs.append(f'{region} {selected} {pressed} {repeated} {blocked} {mode}')
            expected.append(f'{get(arena+204)} {dispatch[0]} {dispatch[1]} {int(dispatch[2])}')
    actual=subprocess.check_output([str(a.executable),'--batch'],input='\n'.join(inputs)+'\n',text=True).splitlines()
    assert len(actual)==len(expected)
    for line,x,y in zip(inputs,actual,expected):assert x==y,(line,x,y)
    report={'status':'passed','update_cases':len(inputs),'exit_flag_cases':gate_cases,'table_entries':[len(x[1]) for x in tables],'dll_sha256':digest,'functions':['0x100D5600','0x100D5780'],'limits':'Actual input globals supplied. Virtual sound/scene/exit calls captured, not executed. TEST initialization, subscenes, AVS/EA3/hardware not exercised.'}
    if a.report.exists():raise ValueError('Report already exists')
    a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS',len(inputs),'original TEST updates and exit-flag evaluations; both original tables')
if __name__=='__main__':main()
