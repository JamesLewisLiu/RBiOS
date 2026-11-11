"""Compare native mount lookup/path mapping with unchanged original instructions."""
import argparse,hashlib,json,os,random,struct,subprocess,sys
from pathlib import Path
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('game-root','dependencies','executable','report'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args()
    if a.report.exists():raise ValueError('Report already exists')
    for name in ('modules','data'):
        if a.report.resolve().is_relative_to((a.game_root/name).resolve()):raise ValueError('Report must be outside assets')
    sys.path.insert(0,str(a.dependencies))
    import pefile
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
    from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EDX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
    raw=(a.game_root/'modules/libavs-win32.dll').read_bytes();digest=hashlib.sha256(raw).hexdigest()
    assert digest=='d062780e88927b442eb6443f310d9b9f692ce6426e3bbf7e284e9d0a15bdfc91'
    pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
    mem,stack,end=0x7000000,0x8000000,0x9000000
    for address in (mem,stack,end):u.mem_map(address,65536)
    def call(address,args=(),eax=0,edx=0,ecx=0):
        sp=stack+60000;u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),end,*args))
        for reg,value in ((UC_X86_REG_ESP,sp),(UC_X86_REG_EAX,eax),(UC_X86_REG_EDX,edx),(UC_X86_REG_ECX,ecx)):u.reg_write(reg,value)
        u.emu_start(address,end,count=100000);assert u.reg_read(UC_X86_REG_EIP)==end
        return u.reg_read(UC_X86_REG_EAX)
    def canonical(path):
        u.mem_write(mem,b'\0');u.mem_write(mem+1024,path.encode()+b'\0')
        status=call(0x1002c110,(0,0,0,256,mem+2048),eax=mem,edx=mem+1024,ecx=mem+4096)
        assert status<0x80000000
        depth=status>>16;length=status&65535
        return bytes(u.mem_read(mem+4096,length)),list(struct.unpack('<'+'I'*(depth+1),u.mem_read(mem+2048,4*(depth+1))))
    rng=random.Random(0x2c820);rows=[];expected=[]
    paths=['/','/dev','/dev/raw','/dev/nvram','/data','/data/music','/Aa','/BB','/alphabet','/alpha']
    queries=['/','/dev','/dev/raw','/dev/raw/a','/dev/rawish/a','/data/music/../file','/Aa/x','/BB/y','/alphabet/z','/alpha/z','/absent']
    for i in range(500):
        mounts=rng.sample(paths,rng.randrange(1,len(paths)+1));query=rng.choice(queries)
        if i%3==0:query+='/child/file.ifs'
        u.mem_write(0x1007bec0,b'\0'*(65*8*4));devices={}
        for j,path in enumerate(mounts):
            normalized,hashes=canonical(path);depth=len(hashes)-1
            record=mem+8192+j*1024;u.mem_write(record,b'\0'*1024)
            bucket=0x1007bec0+4*(depth*8+(hashes[-1]&7))
            head=struct.unpack('<I',u.mem_read(bucket,4))[0]
            u.mem_write(record+12,struct.pack('<I',head));u.mem_write(bucket,struct.pack('<I',record))
            u.mem_write(record+540,struct.pack('<IHH',hashes[-1],len(normalized),depth)+normalized+b'\0')
            device=f'device{j}';devices[record]=device;u.mem_write(record+16,device.encode()+b'\0')
        normalized,hashes=canonical(query);query_struct=mem+24576
        u.mem_write(query_struct,b'\0'*1024)
        u.mem_write(query_struct,struct.pack('<'+'I'*len(hashes),*hashes))
        u.mem_write(query_struct+260,struct.pack('<HH',len(normalized),len(hashes)-1)+normalized+b'\0')
        selected=call(0x1002c820,eax=query_struct)
        if not selected:expected.append('-2147024877 -')
        else:
            suffix=call(0x1002bd00,(query_struct,selected+540))
            result=call(0x1002a8b0,(0,selected+16,suffix,mem+28672,256))
            assert result==0
            host=bytes(u.mem_read(mem+28672,256)).split(b'\0',1)[0]
            if os.name!='nt':host=host.replace(b'\\',b'/')
            expected.append('0 '+host.hex())
        rows.append(str(len(mounts))+''.join(' '+path.encode().hex()+' '+f'device{j}'.encode().hex() for j,path in enumerate(mounts))+' '+query.encode().hex())
    actual=subprocess.check_output([str(a.executable),'--batch'],input='\n'.join(rows)+'\n',text=True).splitlines()
    assert len(actual)==len(expected)
    for i,(got,want) in enumerate(zip(actual,expected)):assert got==want,f'{i}: {got} != {want}'
    a.report.write_text(json.dumps({'status':'passed','cases':len(rows),'dll_sha256':digest,
        'addresses':['0x1002C110','0x1002C820','0x1002BD00','0x1002A8B0','0x1002A900'],
        'compared':'Longest-depth lookup, hash collisions and prefix boundaries, mount-relative suffix and host device concatenation',
        'limits':'Original canonicalizer/lookup/suffix/join instructions execute without hooks. Mount records are synthetic; actual registration, mounting, descriptor lifetime and OS file I/O are not executed by this oracle. POSIX host separator adaptation is explicit.'},indent=2)+'\n')
    print(f'PASS {len(rows)} unchanged original mount lookup and path mapping comparisons')
if __name__=='__main__':main()
