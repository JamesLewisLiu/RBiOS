"""Compare completion registration and live dispatch against original x86 instructions."""
import argparse, hashlib, json, random, struct, subprocess, sys
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['dll','dependencies','executable','tls-executable','report']:p.add_argument('--'+name,type=Path,required=True)
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
    callbacks=[0]+[end+4096+i*512 for i in range(8)];changes=[[] for _ in range(9)];trace=[]
    imports={};tls_value=0
    for table in pe.DIRECTORY_ENTRY_IMPORT:
        for symbol in table.imports:
            if symbol.name in [b'TlsGetValue',b'GetCurrentThread']:
                address=end+64+len(imports)*16;imports[address]=symbol.name;put(symbol.address,address)
    def compile_callback(index):
        address=callbacks[index];code=bytearray()
        for operation,target in changes[index]:
            code+=b'\x68'+struct.pack('<I',callbacks[target])+b'\x68'+struct.pack('<I',arena)
            destination=0x10041960 if operation=='A' else 0x10041930
            code+=b'\xe8'+struct.pack('<i',destination-(address+len(code)+5))+b'\x83\xc4\x08'
        code+=b'\xc3';assert len(code)<512
        u.mem_write(address,bytes(code));u.ctl_remove_cache(address,address+512)
    for index in range(1,9):compile_callback(index)
    def hook(uc,address,size,data):
        if address in callbacks[1:]:trace.append(f'{callbacks.index(address)}:{get(uc.reg_read(UC_X86_REG_ESP)+4)}')
        elif address in imports or address in [0x10029220,0x1000a980]:
            sp=uc.reg_read(UC_X86_REG_ESP);is_tls=imports.get(address)==b'TlsGetValue'
            uc.reg_write(UC_X86_REG_EAX,tls_value if is_tls else 0)
            uc.reg_write(UC_X86_REG_EIP,get(sp));uc.reg_write(UC_X86_REG_ESP,sp+4+(4 if is_tls else 0))
    u.hook_add(UC_HOOK_CODE,hook)
    def run(address,*args):
        sp=stack+60000;put(sp,end)
        for i,value in enumerate(args):put(sp+4+4*i,value)
        u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(address,end,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==end
        return u.reg_read(UC_X86_REG_EAX)
    commands=[];expected=[];rng=random.Random(0x41960)
    def command(op,value=0,target=0,change='A'):
        if op in ['A','R']:
            result=run(0x10041960 if op=='A' else 0x10041930,arena,callbacks[value])
            expected.append(' '.join(map(str,[result]+[callbacks.index(get(arena+4*i)) for i in range(4)])))
            commands.append(f'{op} {value}')
        elif op=='D':
            trace.clear();run(0x100418c0,arena,value);expected.append('D'+(' '+' '.join(trace) if trace else ''));commands.append(f'D {value}')
        elif op=='M':
            changes[value].append((change,target));compile_callback(value);expected.append('M');commands.append(f'M {value} {change} {target}')
        else:
            for index in range(1,9):changes[index].clear();compile_callback(index)
            expected.append('Z');commands.append('Z')
    # Deliberately create a duplicate behind a hole, then mutate a later slot
    # from the first callback. Dispatch must perform live loads, not a snapshot.
    for index in range(1,5):command('A',index)
    command('R',1);command('A',2);command('D',0x40003);command('R',2);command('A',1)
    command('M',1,2,'R');command('M',1,5,'A');command('D',0x40003);command('Z')
    for iteration in range(4096):
        if iteration%32==0:
            command('Z')
            for _ in range(4):command('M',rng.randrange(1,9),rng.randrange(9),rng.choice(['A','R']))
        operation=rng.choice(['A','R','D'])
        command(operation,rng.getrandbits(31) if operation=='D' else rng.randrange(9))
    actual=subprocess.check_output([str(a.executable),'--batch'],input='\n'.join(commands)+'\n',text=True,timeout=30).splitlines()
    assert actual==expected,next(((i,commands[i],x,y) for i,(x,y) in enumerate(zip(actual,expected)) if x!=y),(len(actual),len(expected)))
    self_expected=[str(run(0x1001a9f0))]
    tls_value=arena;put(arena+12,0);self_expected.append(str(run(0x1001a9f0)))
    put(arena+12,arena+256)
    for token in [0,0x40003,0x7f840004]:put(arena+256+32,token);self_expected.append(str(run(0x1001a9f0)))
    self_actual=subprocess.check_output([str(a.tls_executable),'--self'],text=True,timeout=30).splitlines()
    assert self_actual==self_expected,(self_actual,self_expected)
    a.report.write_text(json.dumps({'status':'passed','commands':len(commands),'dll_sha256':digest,
        'self_query_cases':len(self_expected),'functions':['10041930','10041960','100418C0','1001A9F0'],
        'limits':'Original registration/removal/dispatch instructions execute unchanged. Synthetic callbacks invoke original mutation leaves during dispatch; native executable uses actual function pointers. Self-query hooks OS TLS retrieval, current-thread handle and diagnostics only. No external process or network executes.'},indent=2)+'\n',encoding='utf-8')
    print('PASS',len(commands),'original callback registry commands including live dispatch mutations')
if __name__=='__main__':main()
