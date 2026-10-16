"""Original short-hold missed-head boundary, isolated numeric scheduler/dispatch.
A ten-tick hold may complete on relative tick -9 before the initial miss condition
(-10), so the missed head is never submitted. No CPU result/touch is injected;
manual input query explicitly reports no touch. Presentation is unused.
"""
import json,struct
from original_score_fixture import *
from unicorn.x86_const import UC_X86_REG_FPCW
u.reg_write(UC_X86_REG_FPCW,0x027f)
u.mem_map(0,65536)
def check(duration):
    u.mem_write(note,bytes(448));u.mem_write(chart,bytes(404));u.mem_write(vtable,bytes(256))
    put(note,vtable);put(note+160,chart);put(note+112,note)
    put(note+116,0x10159b30);put(note+296,duration);put(note+312,6);put(note+364,-1)
    u.mem_write(note+20,b'\1');u.mem_write(note+141,b'\1')
    u.mem_write(note+280,struct.pack('<f',1.0));put(chart+288,1);put(chart+396,0)
    put(vtable+188,0x10158550)
    # First-press query fixture: user supplies no contact. Original miss query runs.
    u.mem_write(end+0x500,b'\x31\xc0\xc3');put(vtable+184,end+0x500)
    call(0x1016aca0,eax=0,args=(0,))
    output=scratch+0xa000;events=[];states=[]
    for relative in range(-1,-20,-1):
     put(note+192,relative)
     call(0x10008d60,esi=note+112)
     found=bool(call(0x1016b940,ecx=0,edx=0,args=(note,output,output+4))&255)
     completed=u.mem_read(note+308,1)[0];released=u.mem_read(note+409,1)[0]
     states.append({'relative':relative,'completed':completed,'released':released,'style':struct.unpack('<i',u.mem_read(note+312,4))[0]})
     if found:
      grade,reason=struct.unpack('<ii',u.mem_read(output,8));events.append({'relative':relative,'grade':grade,'reason':reason});break
    assert len(events)==1 and events[0]['reason']==6 and events[0]['grade']==4,events
    return {'duration_ticks':duration,'events':events,'missed_head_is_absent':True}
cases=[check(duration) for duration in range(1,12)]
report={'dll_sha256':digest,'original_functions':['10159B30','10008D60','10159AF0','10158550','1016B940'],'cases':cases,'missed_head_is_absent_for_duration_1_through_11':True,'limitations':['firstPress virtual query explicitly returns false','chart is a bounded single hold, CPU absent','does not validate whole-stage presentation']}
(ROOT/'.build/short-hold-boundary.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report))
