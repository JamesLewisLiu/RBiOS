"""Compare exported-symbol samples against upstream AP2/GE2D interpretation.

Reference source is local .tools/afp-reference at the documented fixed commit.
All resource decoding remains in memory. This is not a full movie/runtime test.
"""
from pathlib import Path
import copy
import json
import subprocess
import sys
import types

ROOT=Path(__file__).resolve().parents[1]
base=ROOT/'.tools/afp-reference'
for name,path in [('bemani',base/'bemani'),('bemani.format',base/'bemani/format'),('bemani.format.afp',base/'bemani/format/afp')]:
    m=types.ModuleType(name);m.__path__=[str(path)];sys.modules[name]=m
sys.path.insert(0,str(ROOT/'.tools'))
from bemani.format.afp.swf import SWF
from bemani.format.afp.geo import Shape
from bemani.format.afp.types import Matrix,Point
from ifstools import IFS
from kbinxml import KBinXML

package=sys.argv[1] if len(sys.argv)>1 else 'gmobject'
assert package in ['gmobject','gmbg_00','gmbg_01','gmbg_02']
path=ROOT.parent/f'data/graphics_rb6/{package}.ifs'
archive=IFS(str(path),super_disable=True)
files={p.full_path.replace('\\','/'):p for p in archive.tree.all_files}
xml=KBinXML(next(f for name,f in files.items() if 'texturelist' in name).load(convert_kbin=False)).xml_doc
uv_regions={}
for sheet in xml:
    size=list(map(int,sheet.find('size').text.split()))
    for image in sheet.findall('image'):
        rect=list(map(int,image.find('uvrect').text.split()))
        uv_regions[image.attrib['name']]=[(rect[x]/(2*size[0]),rect[y]/(2*size[1])) for x,y in [(0,2),(1,2),(0,3),(1,3)]]
rows=[];expected=[]
label_rows=[];label_expected=[]
names=['gmbg_00','gmbg_01'] if package.startswith('gmbg_') else ['obj_bigbang_big_red','obj_bigbang_big_blue','obj_bigbang_small_red','obj_bigbang_small_blue','swich_on','swich_off']
for name in names:
    movie=SWF(name,files['afp/'+name].load(convert_kbin=False),files['afp/bsi/'+name].load(convert_kbin=False));movie.parse()
    definitions={}
    def index(timeline):
        for t in timeline.tags:
            if t.id is not None:definitions[t.id]=t
            if hasattr(t,'tags'):index(t)
    index(movie)
    def sample(id,frame,parent,color,blend=0):
        symbol=definitions[id]
        if hasattr(symbol,'tags'):
            display={}
            for f in range(frame+1):
                spec=symbol.frames[f]
                for t in symbol.tags[spec.start_tag_offset:spec.start_tag_offset+spec.num_tags]:
                    if type(t).__name__=='AP2DoActionTag':continue
                    if type(t).__name__!='AP2PlaceObjectTag':raise AssertionError(type(t).__name__)
                    if t.update:
                        old,born=display[t.depth]
                        if t.source_tag_id is not None:old.source_tag_id=t.source_tag_id
                        if t.transform is not None:old.transform=old.transform.update(t.transform,False)
                        if t.rotation_origin is not None:old.rotation_origin=t.rotation_origin
                        if t.mult_color is not None:old.mult_color=t.mult_color
                        if t.add_color is not None:old.add_color=t.add_color
                        if t.blend is not None:old.blend=t.blend
                    else:display[t.depth]=(copy.deepcopy(t),f)
            result=[]
            for depth,(p,born) in sorted(display.items()):
                transform=(p.transform or Matrix.identity()).multiply(parent).translate(Point.identity().subtract(p.rotation_origin or Point.identity()))
                mul=p.mult_color
                c=[color[i]*([mul.r,mul.g,mul.b,mul.a][i] if mul else 1) for i in range(4)]
                target=definitions[p.source_tag_id]
                age=frame-born
                if hasattr(target,'frames'):age%=len(target.frames)
                result.extend(sample(p.source_tag_id,age,transform,c,p.blend or blend))
            return result
        if color[3]==0:return []
        shape=Shape(symbol.reference,files['geo/'+symbol.reference].load(convert_kbin=False));shape.parse()
        assert len(shape.draw_params)==1
        param=shape.draw_params[0];assert param.flags==3
        assert len(shape.tex_points)==4
        for uv,want_uv in zip(shape.tex_points,uv_regions[param.region]):
            assert abs(uv.x-want_uv[0])<0.0000001 and abs(uv.y-want_uv[1])<0.0000001,(name,param.region,uv,want_uv)
        return [[param.region,parent.a,parent.b,parent.c,parent.d,parent.tx,parent.ty,shape.bounds.left,shape.bounds.right,shape.bounds.top,shape.bounds.bottom,*color,blend]]
    for frame in range(len(definitions[movie.exported_tags[name]].frames)):
        rows.append(f'{name} {frame}')
        expected.append(sample(movie.exported_tags[name],frame,Matrix.identity(),[1,1,1,1]))
    if name.startswith('swich_'):
        sprite=definitions[movie.exported_tags[name]]
        def playback(label,elapsed):
            frame=sprite.labels[label]-1
            # Independent interpretation of parsed upstream AP2 actions, not raw C++ bytes.
            for tick in range(elapsed+1):
                jumps=0
                while True:
                    stop=False;goto=None
                    spec=sprite.frames[frame]
                    for tag in sprite.tags[spec.start_tag_offset:spec.start_tag_offset+spec.num_tags]:
                        if type(tag).__name__!='AP2DoActionTag':continue
                        data=tag.as_dict()['bytecode']['actions']
                        objects=[v for action in data for v in action.get('objects',[])]
                        if 'StringConstant(0x440: stop)' in objects:stop=True
                        elif 'StringConstant(0x815: deepGotoAndPlay)' in objects:
                            goto=next(v[1:-1] for v in objects if v.startswith("'"))
                        else:raise AssertionError(objects)
                    if goto is None:break
                    frame=sprite.labels[goto]-1;jumps+=1;assert jumps<64
                if stop or tick==elapsed:return frame
                frame=(frame+1)%len(sprite.frames)
        for label in ['off','on','loop']:
            for tick in [*range(130),240,1000,10000]:
                label_rows.append(f'{name} {label} {tick}')
                label_expected.append(sample(movie.exported_tags[name],playback(label,tick),Matrix.identity(),[1,1,1,1]))
exe=ROOT/'.build'/('rbafptimeline.exe' if sys.platform=='win32' else 'rbafptimeline')
def compare(mode,rows,expected):
    lines=subprocess.check_output([str(exe),mode,str(path)],input=('\n'.join(rows)+'\n').encode()).decode().splitlines()
    assert len(lines)==len(rows)
    commands=0
    for row,line,want in zip(rows,lines,expected):
        parts=line.split();count=int(parts.pop(0));assert count==len(want),(row,count,len(want))
        for item in want:
            assert parts.pop(0)==item[0],row
            for value in item[1:]:
                actual=float(parts.pop(0))
                assert abs(actual-value)<0.0002,(row,actual,value)
            commands+=1
        assert not parts
    return commands
commands=compare('--sample',rows,expected)
label_commands=compare('--label',label_rows,label_expected)
archive.close()
report={'reference_commit':'1e66fcd780afb3088ce8e78c88615a143db492ee','package':package,'symbol_frames':len(rows),'draw_commands':commands,'label_playback_samples':len(label_rows),'label_draw_commands':label_commands,'mode':'exported-symbol sampling with known stop/goto scripts; not global movie/aeplib execution'}
(ROOT/f'.build/afp-{package}-timeline-parity.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('PASS',len(rows),'exported symbol frames,',commands,'original texture/matrix/bounds/color commands;',len(label_rows),'label playback samples,',label_commands,'commands')
