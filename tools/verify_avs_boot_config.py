"""Compare AVS2 product-mode correction with original libavs instructions."""
import argparse, hashlib, json, random, struct, subprocess, sys
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
        if a.report.resolve().is_relative_to((a.game_root/name).resolve()): raise ValueError('Report must be outside original assets')
    sys.path.insert(0,str(a.dependencies.resolve()))
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP
    raw=(a.game_root/'modules/libavs-win32.dll').read_bytes();digest=hashlib.sha256(raw).hexdigest()
    assert digest=='d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
    config,stack,end=0x7000000,0x8000000,0x9000000
    for addr in (config,stack,end):u.mem_map(addr,65536)
    logs=[]
    # The only replaced function is formatted logging; the config mutation
    # instructions execute unchanged, with original EAX usercall input.
    fields={0x10068afc:0,0x10068bec:1,0x10068e58:2}
    def hook(u,address,size,data):
        if address!=0x1000a980:return
        sp=u.reg_read(UC_X86_REG_ESP)
        ret,category,fmt,field,previous,replacement=struct.unpack('<6I',u.mem_read(sp,24))
        assert field in fields and replacement==0
        previous=struct.unpack('<i',struct.pack('<I',previous))[0]
        logs.append((fields[field],previous))
        u.reg_write(UC_X86_REG_EAX,123)
        u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    u.hook_add(UC_HOOK_CODE,hook)
    rng=random.Random(0x13b30);rows=[];expected=[]
    for enabled in (0,1):
        for mask in range(8):
            for variant in range(16):
                before=bytearray(rng.getrandbits(8) for _ in range(2528))
                before[1]=rng.randint(1,255) if mask&1 else 0
                struct.pack_into('<H',before,118,rng.randint(1,65535) if mask&2 else 0)
                struct.pack_into('<I',before,2268,rng.randint(1,0xffffffff) if mask&4 else 0)
                logs.clear();u.mem_write(config,bytes(before))
                if enabled:
                    sp=stack+60000;u.mem_write(sp,struct.pack('<I',end));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EAX,config)
                    u.emu_start(0x10013b30,end,count=1000);assert u.reg_read(UC_X86_REG_EIP)==end
                actual=bytes(u.mem_read(config,2528));log_mask=sum(1<<field for field,_ in logs)
                rows.append(f'{enabled} {before.hex()}')
                expected.append(f'{log_mask} {actual.hex()}'+''.join(f' {field}:{value}' for field,value in logs))
    actual=subprocess.check_output([str(a.executable),'--batch'],input='\n'.join(rows)+'\n',text=True).splitlines()
    assert len(actual)==len(expected)
    for i,(got,want) in enumerate(zip(actual,expected)):
        assert got==want, f'case {i} mismatch'
    a.report.parent.mkdir(parents=True,exist_ok=True)
    a.report.write_text(json.dumps({'status':'passed','cases':len(rows),'dll_sha256':digest,'address':'0x10013B30','compared':'All 2528 config bytes and ordered correction log fields/values','limits':'Only formatted logging stubbed. Product-mode dispatch is supplied as input; property import and full boot are not executed.'},indent=2)+'\n')
    print(f'PASS {len(rows)} original AVS product-mode configuration/log comparisons')
if __name__=='__main__':main()
