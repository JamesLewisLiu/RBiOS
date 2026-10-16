"""Original table + isolated x86 pool initializer/acquire/release evidence.

Virtual SetVisible is an explicit recording stub. No movie or game is launched.
"""
import json
import struct
import subprocess
import sys
from original_score_fixture import ROOT, pe, base, digest, u, scratch, end, call, put
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX
image=pe.get_memory_mapped_image()
def text(address):
 at=address-base
 if not 0<=at<len(image):return None
 return image[at:image.find(b'\0',at)].decode('ascii')
rows=[]
for index in range(37):
 values=struct.unpack_from('<7I',image,0x36bfd8+28*index)
 rows.append(dict(index=index,address=hex(base+0x36bfd8+28*index),capacity=values[0],package=values[1],movie=text(values[2]),alternative=text(values[3]),absolutePriority=bool(values[4]&255),layer=values[5],creationOption=values[6]))
expected={23:('swich_on',64),24:('swich_off',64),25:('swich_on',24),26:('swich_off',24),27:('swich_long_body_on',64),28:('swich_long_body_off',64),29:('long_tail_bg_switch',24),31:('obj_bigbang_big_red',24),32:('obj_bigbang_small_red',24)}
checks=0
manager=scratch+0x4000;slots=scratch+0x6000;images=scratch+0x7000;vtable=scratch+0x8000
put(0x124792a0,manager)
u.mem_write(end+0x600,b'\xc2\x04\x00')
hidden=[]
def hook(uc,address,size,user):
 if address==end+0x600:
  sp=uc.reg_read(UC_X86_REG_ESP)
  hidden.append((uc.reg_read(UC_X86_REG_ECX),struct.unpack('<I',uc.mem_read(sp+4,4))[0]))
u.hook_add(UC_HOOK_CODE,hook)
def get(address):return struct.unpack('<I',u.mem_read(address,4))[0]
for index,(movie,capacity) in expected.items():
 row=rows[index];assert (row['movie'],row['capacity'])==(movie,capacity);checks+=1
 u.mem_write(manager,b'\0'*1024);put(vtable+16,end+0x600)
 pool=manager+20+24*index
 put(pool+4,slots);put(pool+16,capacity)
 for slot in range(capacity):put(slots+8*slot,images+16*slot);put(images+16*slot,vtable)
 call(0x1001ef40,ecx=pool)
 assert get(pool+8)==slots and get(pool+12)==0;checks+=1
 for slot in range(capacity):
  assert get(slots+8*slot+4)==(slots+8*(slot+1) if slot+1<capacity else 0);checks+=1
 actors=[]
 for slot in range(capacity):
  actor=scratch+0x1000+128*slot;u.mem_write(actor,b'\0'*120);actors.append(actor)
  assert call(0x10164c30,edi=actor,esi=index)==images+16*slot;checks+=1
 assert get(pool+8)==0;checks+=1
 failure_actor=scratch+0x1000+128*capacity;u.mem_write(failure_actor,b'\0'*120)
 assert call(0x10164c30,edi=failure_actor,esi=index)==0;checks+=1
 released=[2,0,capacity-1];hidden.clear()
 for slot in released:call(0x10164bc0,[actors[slot]])
 assert hidden==[(images+16*slot,0) for slot in released];checks+=1
 for slot in reversed(released):
  assert call(0x10164c30,edi=actors[slot],esi=index)==images+16*slot;checks+=1
exe=ROOT/'.build'/('rboriginalafpmoviepool.exe' if sys.platform=='win32' else 'rboriginalafpmoviepool')
lines=subprocess.check_output([str(exe),'--table'],text=True).splitlines()
assert len(lines)==len(rows);checks+=1
for row,line in zip(rows,lines):
 fields=line.split()
 actual=[int(v) for v in fields[:5]]+fields[5:7]+[int(fields[7])]
 expected_row=[row['index'],row['capacity'],row['package'],row['layer'],row['creationOption'],row['movie'],row['alternative'],int(row['absolutePriority'])]
 assert actual==expected_row,(actual,expected_row);checks+=1
report={'dll_sha256':digest,'checks':checks,'table':rows,'allocation':'initial slots ascend; pop free head; release pushes head (LIFO)','fixtures':['SetVisible virtual records a zero argument and returns'],'limitations':'does not execute complete AFP movies; original diagnostic allocation counter increments even on exhaustion'}
(ROOT/'.build/original-afp-pool.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS original AFP pools',checks,'checks;',len(rows),'table records')
