"""Bounded original x86 touch functions; memory fixtures only, no OS or networking."""
from pathlib import Path
import hashlib,json,random,struct,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.tools'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
payload=(ROOT.parent/'modules/reflecbeat.dll').read_bytes();digest=hashlib.sha256(payload).hexdigest()
assert digest=='ed0ce52a403b57be4c33ee1d87f8dca6c4c4dc71d8d2ad0107216bc8866c0fa8'
pe=pefile.PE(data=payload);base=pe.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
scratch,stack,end=0x7000000,0x8000000,0x9000000
for address in (scratch,stack,end):u.mem_map(address,65536)
word=lambda n:struct.pack('<I',n&0xffffffff)
def put(a,n):u.mem_write(a,word(n))
def call(va,args=(),**regs):
 sp=stack+60000;u.mem_write(sp,word(end)+b''.join(word(n) for n in args));u.reg_write(UC_X86_REG_ESP,sp)
 for name,reg in [('eax',UC_X86_REG_EAX),('ebx',UC_X86_REG_EBX),('ecx',UC_X86_REG_ECX),('edx',UC_X86_REG_EDX),('esi',UC_X86_REG_ESI),('edi',UC_X86_REG_EDI)]:u.reg_write(reg,regs.get(name,0)&0xffffffff)
 u.emu_start(va,end,count=15000);assert u.reg_read(UC_X86_REG_EIP)==end,'instruction limit reached'
 return u.reg_read(UC_X86_REG_EAX)&255
note,chart,contacts,manager,vtable,remote,rect,outx,outy,reason=[scratch+i*0x1000 for i in range(10)]
put(0x1247901c,manager);put(manager+212,contacts-4);put(0x12477e84,remote)
put(vtable+168,0x101586b0);put(vtable+172,end+0x300)
call(0x100a4c90)
def iw(r,o,n):struct.pack_into('<i',r,o,n)
def fw(r,o,n):struct.pack_into('<f',r,o,n)
def ri(r,o):return struct.unpack_from('<i',r,o)[0]
rng=random.Random(2019121100)
def fixture():
 r=bytearray(1976)
 for o,n in [(100,rng.randrange(0,768)),(104,rng.choice([500,975,976,1000,1100,1222])),(192,rng.randint(-1100,10)),(216,rng.choice([-1,1,2,3])),(296,rng.randrange(200)),(300,rng.randrange(5)),(304,rng.randrange(8)),(312,rng.randrange(7)),(360,rng.randrange(200)),(364,rng.choice([-1,0,1,2])),(368,rng.randrange(200)),(372,rng.randrange(2)),(384,rng.choice([976,1100])),(388,1200),(392,-9),(396,9),(400,rng.randrange(200)),(420,rng.choice([0,0,0,1])),(448+16,rng.randint(-5,10)),(448+284,rng.randrange(2)),(448+288,rng.choice([0,1,2,4])),(448+328,rng.choice([0,0,0,1])),(448+400,rng.randrange(3))]:iw(r,o,n)
 for o in [376,408,411,448+357]:r[o]=rng.choice([0,0,0,1])
 for k in range(15):
  o=864+64*k;r[o]=rng.choice([0,1]);iw(r,o+4,k+1);iw(r,o+24,ri(r,100)+rng.randint(-200,200));iw(r,o+28,ri(r,104)+rng.randint(-130,130));r[o+32]=rng.randrange(4)
  for j in [53,54,56]:r[o+j]=rng.choice([0,0,0,1])
 iw(r,1824,-77);fw(r,1828,rng.choice([.25,.5,.9,1.,1.1]));iw(r,1832,rng.randrange(2));iw(r,1836,rng.randrange(2));iw(r,1840,rng.randrange(2));iw(r,1844,3)
 for k in range(3):iw(r,1848+8*k,ri(r,100)+rng.randint(-150,150));iw(r,1852+8*k,ri(r,104)+rng.randint(-100,100))
 return r
commands=[];expected=[];counts={}
def trial(op,r,rectangle=None):
 u.mem_write(note,bytes(r[:448]));u.mem_write(chart,bytes(r[448:864]));u.mem_write(contacts,bytes(r[864:1824]));put(note,vtable);put(note+160,chart)
 put(0x12479018,ri(r,1824));u.mem_write(0x1036bf0c,bytes(r[1828:1832]));u.mem_write(0x12477d72,bytes([bool(ri(r,1832))]));u.mem_write(remote+12872,bytes([bool(ri(r,1836))]))
 u.mem_write(end+0x300,b'\xb0'+bytes([bool(ri(r,1840))])+b'\xc2\x08\x00');u.ctl_remove_cache(end+0x300,end+0x310)
 put(manager+220,ri(r,1844));u.mem_write(manager+224,bytes(r[1848:]));put(reason,-77)
 if op=='H':yes=call(0x10158ba0,esi=note)
 elif op=='V':yes=call(0x10159d10,eax=note,args=(ri(r,372),))
 elif op=='S':yes=call(0x10159610,edi=note,args=(reason,))
 elif op=='F':yes=call(0x10159820,ecx=note)
 elif op=='M':yes=call(0x10158550,ecx=note)
 elif op=='J':
  put(vtable+172,0x101584b0);yes=call(0x10158550,ecx=note);put(vtable+172,end+0x300)
 elif op=='G':yes=call(0x101584b0,ecx=note,args=rectangle)
 elif op=='W':yes=call(0x10158750,ecx=note)
 elif op=='X':yes=call(0x101586b0,ecx=note)
 else:
  x,y,w,h=rectangle;u.mem_write(rect,struct.pack('<4i',*rectangle))
  if op=='B':yes=call(0x10061630,eax=rect,args=(manager,))
  elif op=='U':yes=call(0x10061520,edx=manager,edi=rect)
  else:
   put(outx,ri(r,428));put(outy,ri(r,432));yes=call(0x10061cb0,eax=manager,args=(x,y,x+w,y+h,1,1,outx,outy))
   u.mem_write(note+428,bytes(u.mem_read(outx,4)));u.mem_write(note+432,bytes(u.mem_read(outy,4)))
 output=bytearray(r);output[:448]=u.mem_read(note,448);output[448:864]=u.mem_read(chart,416);output[864:1824]=u.mem_read(contacts,960);output[1824:1828]=u.mem_read(0x12479018,4)
 output[:4]=r[:4];output[160:164]=r[160:164]
 reasonvalue=struct.unpack('<i',u.mem_read(reason,4))[0]
 commands.append(f'{op} {r.hex()}'+(' '+' '.join(map(str,rectangle)) if rectangle else ''));expected.append(f'{yes} {reasonvalue} {output.hex()}');counts[op]=counts.get(op,0)+1
for op in 'HV S FMWX BPU'.replace(' ',''):
 for _ in range(150):
  r=fixture();trial(op,r,(ri(r,100)-100,ri(r,104)-100,200,200) if op in 'BPU' else None)
# Directed cases force successful first-press, switch capture/release, grace, and miss branches.
for op in 'HVSFMWX':
 for wide in (0,1):
  for tick in (-1001,-501,-10,-9,-1,0,1,9,10):
   for kind in (0,1,4):
    r=fixture()
    for offset,value in [(100,384),(104,1100),(192,tick),(216,-1),(296,100),(300,3),(304,5),(312,6),(360,0),(364,-1),(368,100),(372,0),(384,976),(388,1200),(392,-9),(396,9),(400,0),(420,0),(448+16,0),(448+284,0),(448+288,kind),(448+328,0),(448+400,2)]:iw(r,offset,value)
    for offset in (376,408,411):r[offset]=0
    r[448+357]=wide
    for k in range(15):r[864+64*k]=0
    o=864;r[o]=1;iw(r,o+4,1);iw(r,o+24,384);iw(r,o+28,1100);r[o+32]=3;r[o+53]=r[o+54]=r[o+56]=0
    fw(r,1828,1.0);trial(op,r)
for direction in (-1,0,1,2,3,4):
 for margin in (0,1,18,36,100,-36):
  for radius_scale in (.25,.5,.9,1.,1.1,2.):
   for x,y in ((0,80),(36,116),(35,115),(732,1186),(733,1187),(768,1222),(384,1100)):
    r=fixture();iw(r,100,x);iw(r,104,y);fw(r,1828,radius_scale);trial('G',r,(direction,margin))
for _ in range(300):trial('J',fixture())
exe=ROOT/'.build'/('rbtouch.exe' if sys.platform=='win32' else 'rbtouch')
actual=subprocess.check_output([str(exe),'--batch'],input=('\n'.join(commands)+'\n').encode()).decode().splitlines();assert len(actual)==len(expected)
for k,(a,b) in enumerate(zip(expected,actual)):
 if a!=b:
  av=a.split();bv=b.split();diff=[(i,x,y) for i,(x,y) in enumerate(zip(bytes.fromhex(av[2]),bytes.fromhex(bv[2]))) if x!=y]
  raise AssertionError((k,commands[k][0],av[:2],bv[:2],diff,commands[k]))
report={'dll_sha256':digest,'method':'bounded original x86 touch and note queries; no Windows APIs/network','cases':len(commands),'coverage':counts,'limitations':['M cases isolate +172 with a boolean fixture; J and G cases execute recovered +172 geometry','finite runtime touchScale values only; no Apple touch lifecycle validation','15 original slots; source record allocation/OS input ingestion not migrated']}
(ROOT/'.build/touch-parity.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report))
