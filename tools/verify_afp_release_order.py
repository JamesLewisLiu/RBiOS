"""Execute original note cleanup and pool releases in isolated x86 memory.

Only renderer SetVisible is a recording stub. All cleanup dispatch, component
resets, actor resets, and free-list mutations execute original instructions.
No game process, Windows API, assets, or network transport is invoked.
"""
import json
import struct
from original_score_fixture import ROOT, digest, u, scratch, end, call, put
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP

note=scratch+0x8000
head=scratch+0x8400
switch=scratch+0x8800
long=scratch+0x8c00
aux=scratch+0x9000
manager=scratch+0x9400
actor_list=scratch+0x9800
image_vtable=scratch+0x9900
pool_manager=scratch+0xa000
u.mem_write(scratch+0x2000,bytes(0xc000))
put(0x1247929c,manager)
put(manager+20,actor_list)
put(0x124792a0,pool_manager)
put(image_vtable+16,end+0x600)
u.mem_write(end+0x600,b'\xc2\x04\x00')
put(note,0x1028de34)
put(head,0x1028e1ac)
put(switch,0x1028e134)
put(long,0x1028dd44)
put(note+212,aux)
put(note+264,head)
actors={}
slots={}
pool_releases={}
events=[]


def get(address):
    return struct.unpack('<I',u.mem_read(address,4))[0]


def actor(name,pool):
    index=len(actors)
    address=scratch+0x2000+128*index
    node=scratch+0x7000+8*index
    image=scratch+0x6000+16*index
    put(address,0x1028dc0c)
    put(address+112,pool)
    put(address+116,node)
    put(node,image)
    put(image,image_vtable)
    actors[address]=name
    slots[name]=(pool,node)
    put(pool_manager+32+24*pool,get(pool_manager+32+24*pool)+1)
    put(pool_manager+912,get(pool_manager+912)+1)
    return address


def static_image():
    address=scratch+0x6800
    put(address,image_vtable)
    return address


# Deliberately populate every optional actor, even though one chart note uses
# either long or switch. This tests the outer cleanup order without inference.
for offset,name,pool in [(44,'head.on',23),(48,'head.off',24),
                        (52,'head.bigbang',31),(36,'head.overlay',0),
                        (40,'head.collection',33)]:
    put(head+offset,actor(name,pool))
put(long+112,actor('long.exp_loop',30))
put(long+116,actor('long.tail_effect',15))
for offset in [148,152,156]:put(long+offset,static_image())
for i in range(16):
    put(switch+112+4*i,actor('switch.body.'+str(i),27+i%2))
    put(switch+176+4*i,actor('switch.cap.'+str(i),24-i%2))
    put(switch+240+4*i,actor('switch.collection.'+str(i),33))
put(switch+304,actor('switch.tail_bg',29))
put(switch+308,static_image())
put(note+256,long)
put(note+260,switch)


def trace(uc,address,size,user):
    if address==0x10164bc0:
        sp=uc.reg_read(UC_X86_REG_ESP)
        owner=get(sp+4)
        name=actors[owner]
        events.append(name)
        pool,node=slots[name]
        pool_releases.setdefault(pool,[]).append(node)


u.hook_add(UC_HOOK_CODE,trace)
call(0x101591d0,ecx=note)
expected=['long.exp_loop','long.tail_effect']
expected+=['switch.body.'+str(i) for i in range(16)]
expected+=['switch.cap.'+str(i) for i in range(16)]
expected+=['switch.collection.'+str(i) for i in range(16)]
expected+=['switch.tail_bg','head.on','head.off','head.bigbang',
           'head.overlay','head.collection']
assert events==expected,(events,expected)
checks=1
node_name={node:name for name,(pool,node) in slots.items()}
free_order={}
for pool,released in pool_releases.items():
    current=get(pool_manager+28+24*pool)
    actual=[]
    while current:
        actual.append(current)
        current=get(current+4)
    assert actual==list(reversed(released)),(pool,actual,released)
    free_order[pool]=[node_name[node] for node in actual]
    checks+=1
for address in actors:
    assert get(address+116)==0 and get(address+112)==0xffffffff
    checks+=1
assert get(note+256)==get(note+260)==0
checks+=1
report={
 'dll_sha256':digest,'checks':checks,
 'entry':'0x101591D0 CNoteBase reset (vtable 0x1028DE34 +20)',
 'component_functions':{'ordinary_long':'0x1015BDE0',
                        'switch_long':'0x10156690','head':'0x10155890'},
 'actor_reset':'0x100058D0 -> 0x10164BC0 pool release -> virtual +24 inactive',
 'release_order':events,'next_acquire_order_by_pool':free_order,
 'fixtures':['renderer virtual SetVisible returns after accepting one argument',
             'empty stage actor list','all optional graphics populated'],
 'limitations':'cleanup only; does not execute game scene or complete AFP movies'
}
(ROOT/'.build/original-afp-release-order.json').write_text(
    json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('PASS original AFP cleanup order',checks,'checks;',len(events),'releases')
