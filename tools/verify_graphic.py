"""Original 10155F10 selection differential, isolated DLL instructions only."""
import itertools
from original_score_fixture import *

actor = scratch+0x6000
pool = scratch+0x7000
objects = scratch+0x8000
put(0x124792a0,pool)
stub = end+0x500
u.mem_write(stub,b'\xc3')
put(vtable+20,stub)

def string_at(address):
    if not address:return '-'
    data=bytes(u.mem_read(address,96)).split(b'\0')[0]
    return data.decode('ascii')

inputs=[]; expected=[]
for color,top,kind,side,special,width,flags,presentation in itertools.product(range(2),range(2),range(5),range(2),[0,1],[-1,0,1],range(4),[0,8]):
    row=(color,top,kind,side,special,5,0,width,flags,presentation)
    u.mem_write(actor,bytes(128));u.mem_write(chart,bytes(404));u.mem_write(objects,bytes(640))
    put(actor,vtable)
    for i in range(5):put(actor+36+4*i,objects+128*i)
    for off,value in [(280,color),(284,top),(288,kind),(328,side),(16,special),(352,5),(372,0),(400,width),(300,flags)]:put(chart+off,value)
    call(0x10155f10,[chart,presentation],ecx=actor)
    style=struct.unpack('<i',u.mem_read(actor+32,4))[0]
    head,bg=[string_at(struct.unpack('<I',u.mem_read(actor+off,4))[0]) for off in [4,8]]
    on=off='-'
    if style==7:on,off='swich_on','swich_off'
    elif style in [5,6]:on=('obj_bigbang_big_' if style==5 else 'obj_bigbang_small_')+('blue' if color else 'red')
    overlay,secondary=u.mem_read(actor+12,2)
    inputs.append(' '.join(map(str,row)))
    expected.append(f'{style} {head} {bg} {on} {off} {int(bool(overlay))} {int(bool(secondary))}')

exe=ROOT/'.build'/('rbnotegraphic.exe' if sys.platform=='win32' else 'rbnotegraphic')
actual=subprocess.check_output([str(exe),'--batch'],input=('\n'.join(inputs)+'\n').encode()).decode().splitlines()
assert len(actual)==len(expected)
for index,(a,e) in enumerate(zip(actual,expected)):
    assert a==e,(inputs[index],a,e)
angles=[]; angle_results=[]
for style,side,direction in itertools.product(range(9),range(2),[-6.28,-1.5,0.,1.5,3.14,6.28]):
    bits=struct.unpack('<I',struct.pack('<f',direction))[0]
    put(actor+32,style);put(actor+16,side)
    wrapper=end+0x600; output=end+0x700
    va=0x10155800
    # push float; mov ecx,actor; call thiscall(original consumes float);
    # fstp output; ret. The wrapper records original float rounding.
    code=b'\x68'+word(bits)+b'\xb9'+word(actor)
    code+=b'\xe8'+word(va-(wrapper+len(code)+5))+b'\xd9\x1d'+word(output)+b'\xc3'
    u.mem_write(wrapper,code);u.ctl_remove_cache(wrapper,wrapper+len(code))
    call(wrapper)
    angles.append(f'{style} {side} {bits}')
    angle_results.append(str(struct.unpack('<I',u.mem_read(output,4))[0]))
actual_angles=subprocess.check_output([str(exe),'--angles'],input=('\n'.join(angles)+'\n').encode()).decode().splitlines()
assert actual_angles==angle_results
report={'dll_sha256':digest,'original_entry':'0x10155F10 / 0x10155800','cases':len(inputs),'angle_cases':len(angles),'scope':'original bitmap/style selection; actor resource allocation is empty fixture; not AFP execution'}
(ROOT/'.build/graphic-parity.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('PASS',len(inputs),'original note graphic selections and',len(angles),'original angle cases')
