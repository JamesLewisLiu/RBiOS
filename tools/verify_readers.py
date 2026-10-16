"""Compare in-memory decoded bytes against independent desktop readers."""
from pathlib import Path
import hashlib
from io import BytesIO
import json
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'.tools'))
from ifstools import IFS
from PIL import Image
from imageio_ffmpeg import get_ffmpeg_exe
exe=ROOT/'.build'/('rbinspect.exe' if sys.platform=='win32' else 'rbinspect')
data=ROOT.parent/'data';report={'textures':[],'audio':[]}
paths=list((data/'graphics_characard').glob('*.ifs'))[:2]+list((data/'graphics_rb6').glob('bg_*.ifs'))[:2]
paths += [next(data.rglob(n)) for n in ['4k71.ifs','mj_k00s.ifs']]
for p in paths:
    archive=IFS(str(p),super_disable=True)
    try:
        images=[f for f in archive.tree.all_files if hasattr(f,'imgrect')]
        if not images:continue
        expected=Image.open(BytesIO(images[0].load(use_cache=False))).convert('RGBA').tobytes('raw','BGRA')
        actual=subprocess.check_output([str(exe),'--texture-raw',str(p)])
        if actual!=expected:raise AssertionError(f'texture pixels differ: {p.name}')
        report['textures'].append({'file':p.name,'bytes':len(actual),'sha256':hashlib.sha256(actual).hexdigest()})
    finally:archive.close()
audio=sorted((data/'sound/sys_sd_rb6').glob('*.sd9'))[:3]+sorted((data/'sound/2dx').glob('*.2dx'))[:2]
# Include an actual long song in addition to the small system/dummy samples.
largest=max((data/'sound').rglob('*.2dx'),key=lambda p:p.stat().st_size)
if largest not in audio:audio.append(largest)
for p in audio:
    b=p.read_bytes();off=b.index(b'RIFF');length=struct.unpack_from('<I',b,off+4)[0]+8
    expected=subprocess.run([get_ffmpeg_exe(),'-v','error','-f','wav','-i','pipe:0','-f','s16le','-acodec','pcm_s16le','pipe:1'],input=b[off:off+length],capture_output=True,check=True).stdout
    actual=subprocess.check_output([str(exe),'--audio-raw',str(p)])
    # FFmpeg emits complete blocks; original WAVE fact frames exclude terminal padding.
    if len(expected)<len(actual) or actual!=expected[:len(actual)]:raise AssertionError(f'audio samples differ: {p.name}')
    report['audio'].append({'file':p.name,'bytes':len(actual),'sha256':hashlib.sha256(actual).hexdigest()})
(ROOT/'.build/reader-parity.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(f"PASS {len(report['textures'])} texture comparisons and {len(report['audio'])} PCM comparisons (memory only)")
