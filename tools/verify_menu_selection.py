"""Compare restored difficulty selection with isolated original x86 update code."""
import argparse, hashlib, itertools, json, struct, subprocess, sys
from pathlib import Path

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game-root', type=Path, required=True)
    ap.add_argument('--dependencies', type=Path, required=True)
    ap.add_argument('--executable', type=Path, required=True)
    ap.add_argument('--report', type=Path, required=True)
    a = ap.parse_args()
    if a.report.exists(): raise ValueError('Report already exists')
    sys.path.insert(0, str(a.dependencies.resolve()))
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ECX, UC_X86_REG_EAX
    raw = (a.game_root/'modules/reflecbeat.dll').read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    assert digest == 'ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
    pe = pefile.PE(data=raw); u = Uc(UC_ARCH_X86, UC_MODE_32)
    base = pe.OPTIONAL_HEADER.ImageBase
    u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
    u.mem_write(base, pe.get_memory_mapped_image())
    arena, stack, end = 0x7000000, 0x8000000, 0x9000000
    for address in [arena, stack, end]: u.mem_map(address, 65536)
    window, view, song = arena, arena+1024, arena+2048
    buttons = [arena+3072+i*256 for i in range(3)]
    def put(at, value): u.mem_write(at, struct.pack('<I', value&0xffffffff))
    def get(at): return struct.unpack('<i',u.mem_read(at,4))[0]
    def ret(value=0, pop=0):
        sp=u.reg_read(UC_X86_REG_ESP)
        u.reg_write(UC_X86_REG_EAX,value&0xffffffff)
        u.reg_write(UC_X86_REG_EIP,get(sp)&0xffffffff)
        u.reg_write(UC_X86_REG_ESP,sp+4+pop)
    current = None
    def hook(uc, address, size, user):
        if address == 0x10120D00: ret(current[2])
        elif address == 0x10122570: ret(pop=12)
        elif address == 0x1006BC80: ret()
        elif address == 0x10120E60: ret(pop=4)
        elif address == end+16:
            button=u.reg_read(UC_X86_REG_ECX)
            ret(current[{buttons[0]:4,buttons[1]:5,buttons[2]:6}[button]])
    u.hook_add(UC_HOOK_CODE,hook)
    put(0x10A71F9C,0)
    inputs, expected=[],[]
    for current in itertools.product(range(4),range(16),[-1,0,1,2,3,4],[-1,0,1,4],range(2),range(2),range(2),range(2)):
        selected, mask, panel, cooldown, next_, confirm, detail, debounce=current
        u.mem_write(window,bytes(100)); u.mem_write(view,bytes(256))
        put(window+20,song);put(window+24,selected);put(window+36,view);put(window+84,debounce)
        u.mem_write(window+32,bytes(bool(mask&(1<<i)) for i in range(4)))
        for offset, button in zip([176,168,172],buttons):
            put(view+offset,button);put(button,button+128);put(button+128+80,end+16)
        put(0x124A3F20,cooldown)
        sp=stack+60000;put(sp,end);put(sp+4,0)
        u.reg_write(UC_X86_REG_ECX,window);u.reg_write(UC_X86_REG_ESP,sp)
        u.emu_start(0x1011A170,end,count=300)
        assert u.reg_read(UC_X86_REG_EIP)==end
        expected.append(f'{get(window+24)} {get(0x124A3F20)} {int(get(window+68)==0x10119F00)} {u.mem_read(view+220,1)[0]}')
        inputs.append(' '.join(map(str,current)))
    actual=subprocess.check_output([str(a.executable),'--batch'],input='\n'.join(inputs)+'\n',text=True).splitlines()
    assert len(actual)==len(expected)
    for row,x,y in zip(inputs,actual,expected): assert x==y,(row,x,y)
    sound_ids = struct.unpack('<4I', pe.get_data(0x292624, 16))
    assert sound_ids == (16272, 16352, 16432, 16512)
    sounds = []
    for sound_id, name in zip(sound_ids, ['basic', 'medium', 'hard', 'whitehard']):
        path = a.game_root/'data/sound/sys_sd_rb6'/('sys_vox_'+name+'.sd9')
        data = path.read_bytes()
        assert data[:4] == b'SD9\0' and data[32:36] == b'RIFF'
        assert struct.unpack_from('<H', data, 30)[0] * 80 + 8192 == sound_id
        sounds.append({'id': sound_id, 'path': str(path.relative_to(a.game_root)), 'sha256': hashlib.sha256(data).hexdigest()})
    report={'sound_table': sounds, 'status':'passed' ,'cases':len(inputs),'dll_sha256':digest,'original_function':'0x1011A170','limits':'UI polling, visual refresh, audio and telemetry calls stubbed; original selection, confirmation, detail toggle and cooldown instructions executed. Account eligibility not emulated.'}
    a.report.parent.mkdir(parents=True,exist_ok=True)
    a.report.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS',len(inputs),'original menu updates')
if __name__=='__main__': main()
