"""Isolated original libafp instructions: visibility and playback-rate independence.

The layer lookup is an explicit valid-handle fixture. The advance boundary is
observed before the full movie VM. No game process, resource export or transport.
"""
from pathlib import Path
import hashlib
import json
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.tools'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
path=ROOT.parent/'modules/libafp-win32.dll'
data=path.read_bytes();identity=hashlib.sha256(data).hexdigest()
assert identity=='9542ded21dd85474b8323a36ff019bc5dbae8717d903295c32e36a52b0c93d08','unsupported AFP DLL revision'
pe=pefile.PE(data=data);base=pe.OPTIONAL_HEADER.ImageBase
uc=Uc(UC_ARCH_X86,UC_MODE_32)
uc.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
uc.mem_write(base,pe.get_memory_mapped_image())
STACK=0x20000000;LAYER=0x21000000;STOP=0x22000000
for address,size in [(STACK,65536),(LAYER,4096),(STOP,4096)]:uc.mem_map(address,size)
reached=False

def hook(uc,address,size,user):
 global reached
 if address==base+0x1720: # valid layer-handle lookup fixture
  sp=uc.reg_read(UC_X86_REG_ESP);uc.reg_write(UC_X86_REG_EAX,LAYER)
  uc.reg_write(UC_X86_REG_EIP,struct.unpack('<I',uc.mem_read(sp,4))[0]);uc.reg_write(UC_X86_REG_ESP,sp+4)
 elif address==base+0x6a00: # observation boundary before the complete AFP movie VM
  reached=True;uc.emu_stop()
 elif address==STOP:uc.emu_stop()
uc.hook_add(UC_HOOK_CODE,hook)
def invoke(rva,args=(),eax=0):
 global reached
 reached=False;sp=STACK+0xff00
 uc.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),STOP,*args))
 uc.reg_write(UC_X86_REG_ESP,sp);uc.reg_write(UC_X86_REG_EAX,eax)
 uc.emu_start(base+rva,STOP+1,count=10000)
 return reached
f=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
def dword(offset):return struct.unpack('<I',uc.mem_read(LAYER+offset,4))[0]
uc.mem_write(LAYER,struct.pack('<I',1))
checks=0
invoke(0x4d50,[7,f(1)]) # afp_layer_play(valid handle,1)
assert dword(0x2c)==f(1);checks+=1
invoke(0x7050,[7,1,0]) # clear visibility only
assert dword(0)==0 and dword(0x2c)==f(1);checks+=1
uc.mem_write(base+0x106930,struct.pack('<I',1)) # engine initialized
assert invoke(0x71f0,[0,f(1/60)],LAYER);checks+=1
invoke(0x7050,[7,1,1])
assert dword(0)==1 and dword(0x2c)==f(1);checks+=1
assert invoke(0x71f0,[0,f(1/60)],LAYER);checks+=1
invoke(0x4d50,[7,f(0)])
assert not invoke(0x71f0,[0,f(1/60)],LAYER);checks+=1
report={'dll_sha256':identity,'checks':checks,'observed':'visible and hidden layers with rate1 both reach pre-VM advance boundary; rate0 does not','fixtures':['valid layer lookup at0x10001720','engine initialized flag','stop before full movie VM at0x10006a00'],'not_verified':'movie bytecode execution or iPad renderer'}
(ROOT/'.build/afp-layer-state-parity.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS original AFP layer visibility/rate',checks,'checks; SHA256',identity)
