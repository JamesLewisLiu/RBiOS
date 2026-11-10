"""Compare default offline loop transitions against original 10163CE0 instructions."""
import argparse, hashlib, itertools, json, struct, subprocess, sys
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-root',type=Path,required=True)
    p.add_argument('--dependencies',type=Path,required=True)
    p.add_argument('--executable',type=Path,required=True)
    p.add_argument('--report',type=Path,required=True)
    a=p.parse_args()
    if a.report.exists(): raise ValueError('Report already exists')
    for name in ('data','modules'):
        if a.report.resolve().is_relative_to((a.game_root/name).resolve()):
            raise ValueError('Report must be outside original assets')
    sys.path.insert(0,str(a.dependencies.resolve()))
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_EAX, UC_X86_REG_ECX
    raw=(a.game_root/'modules/reflecbeat.dll').read_bytes()
    digest=hashlib.sha256(raw).hexdigest()
    assert digest=='ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
    pe=pefile.PE(data=raw); u=Uc(UC_ARCH_X86,UC_MODE_32); base=pe.OPTIONAL_HEADER.ImageBase
    u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
    manager,stack,end=0x7000000,0x8000000,0x9000000
    for addr in (manager,stack,end):u.mem_map(addr,65536)
    def put(addr,n):u.mem_write(addr,struct.pack('<I',n))
    calls=[]
    def hook(u,address,size,data):
        if address not in (0x1006b150,0x1006bbe0):return
        reg=UC_X86_REG_ECX if address==0x1006b150 else UC_X86_REG_EAX
        calls.append((address,u.reg_read(reg)))
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    u.hook_add(UC_HOOK_CODE,hook)
    rows=[];expected=[]
    for current,desired in itertools.product((0,1),repeat=2):
        calls.clear();u.mem_write(manager,bytes(104))
        for offset in (60,72,80,88):put(manager+offset,9472)
        put(manager+96,14672 if desired else 9472);put(manager+100,14672 if current else 9472)
        sp=stack+60000;put(sp,end);u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ESI,manager)
        u.emu_start(0x10163ce0,end,count=1000);assert u.reg_read(UC_X86_REG_EIP)==end
        starts=[x for x in calls if x[0]==0x1006bbe0]
        stops=[x for x in calls if x==(0x1006b150,14672)]
        # Original stop(9472) when starting from empty is a no-op sentinel.
        assert len(starts)<=1 and len(stops)<=1
        transition=1 if starts else 2 if stops else 0
        active=struct.unpack('<I',u.mem_read(manager+100,4))[0]==14672
        rows.append(f'{current} {desired}');expected.append(f'{transition} {int(active)}')
    actual=subprocess.check_output([str(a.executable),'--batch'],input='\n'.join(rows)+'\n',text=True).splitlines()
    assert actual==expected,(actual,expected)
    asset=a.game_root/'data/sound/sys_sd_rb6/sys_shot_default1_long_loop.sd9';sound=asset.read_bytes()
    assert struct.unpack_from('<H',sound,30)[0]*80+8192==14672
    assert sound[32:36]==b'RIFF' and struct.unpack_from('<HHI',sound,52)==(1,2,44100)
    a.report.parent.mkdir(parents=True,exist_ok=True)
    a.report.write_text(json.dumps({'status':'passed','transition_cases':len(rows),'dll_sha256':digest,
        'loop_sha256':hashlib.sha256(sound).hexdigest(),'loop_bytes':len(sound),
        'limits':'Default offline SD9 loop lifecycle; hardware calls stubbed. No device audio capture.'},indent=2)+'\n')
    print('PASS four original loop transitions and SD9 loop identity')
if __name__=='__main__':main()
