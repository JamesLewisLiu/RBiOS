"""Compare identity mutations with original x86; no game initialization is run."""
import argparse,hashlib,json,random,struct,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--game-root',type=Path,required=True);p.add_argument('--dependencies',type=Path,required=True);p.add_argument('--executable',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
sys.path.insert(0,str(a.dependencies))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
raw=(a.game_root/'modules/reflecbeat.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest()=='ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
mu=Uc(UC_ARCH_X86,UC_MODE_32);image=pe.get_memory_mapped_image();mu.mem_map(base,(len(image)+4095)&~4095);mu.mem_write(base,image)
mu.mem_map(0x60000000,0x20000)
def word(at):return struct.unpack('<I',mu.mem_read(at,4))[0]
def hook(uc,at,size,data):
    if at not in (0x100307fe,0x1002fafa):return
    sp=uc.reg_read(UC_X86_REG_ESP);dst,cap,src=[word(sp+i) for i in (4,8,12)]
    if at==0x1002fafa:
        n=word(sp+16);assert n<=cap;uc.mem_write(dst,bytes(uc.mem_read(src,n)))
    else:
        payload=bytearray()
        while True:
            c=uc.mem_read(src+len(payload),1)[0];payload.append(c)
            if not c:break
        assert len(payload)<=cap;uc.mem_write(dst,bytes(payload))
    uc.reg_write(UC_X86_REG_EAX,0);uc.reg_write(UC_X86_REG_EIP,word(sp));uc.reg_write(UC_X86_REG_ESP,sp+4)
mu.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(168840);cases=[];expected=[]
for n in range(512):
    initial=bytes(rng.randrange(256) for _ in range(21));sid=bytearray(rng.randrange(1,256) for _ in range(16))
    sid[3]=[0,ord('J'),ord('K'),ord('A')][n%4]
    if n%3==0:sid[6+n%10]=0
    mu.mem_write(0x102aedcc,initial);mu.mem_write(0x60010000,bytes(sid))
    sp=0x6000f000;mu.mem_write(sp,struct.pack('<II',0x6001ffff,0x60010000));mu.reg_write(UC_X86_REG_ESP,sp)
    mu.emu_start(0x10168840,0x10168983,count=10000)
    assert mu.reg_read(UC_X86_REG_EIP)==0x10168983
    expected.append(bytes(mu.mem_read(0x102aedcc,21)).hex()+' '+str(int(sid[3]==ord('K'))))
    cases.append(initial.hex()+' '+bytes(sid).hex())
actual=subprocess.run([str(a.executable),'--batch'],input='\n'.join(cases)+'\n',text=True,capture_output=True,check=True).stdout.splitlines()
assert actual==expected, next(((i,x,y) for i,(x,y) in enumerate(zip(actual,expected)) if x!=y),'output length mismatch')
a.report.write_text(json.dumps({'status':'passed','cases':len(cases),'source_sha256':hashlib.sha256(raw).hexdigest(),'range':['0x10168840','0x10168983'],'stubs':['strcpy_s','memcpy_s'],'not_executed':['1005B580','100B2BD0','101686D0','AVS boot','game main']},indent=2))
print('PASS',len(cases),'original entry identity comparisons')
