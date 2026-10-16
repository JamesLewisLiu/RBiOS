"""Build portable C++ tools; caches/output stay inside this project."""
import os
import hashlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / '.build'
out.mkdir(exist_ok=True)
env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(ROOT/'.cache/zig-global'), ZIG_LOCAL_CACHE_DIR=str(ROOT/'.cache/zig-local'))
zig = ROOT/'.tools/ziglang/zig.exe'
compiler = [str(zig), 'c++'] if zig.exists() else [shutil.which('clang++') or shutil.which('g++') or 'c++']
common = ['-std=c++17', '-O1', '-Wall', '-Wextra', '-Wpedantic', '-ffp-contract=off', '-I'+str(ROOT/'core/include')]
sources = sorted(map(str, (ROOT/'core/src').glob('*.cpp')))
objects = out/'obj'
objects.mkdir(exist_ok=True)
header_identity = b''.join(p.read_bytes() for p in sorted((ROOT/'core/include').rglob('*.hpp')))
build_identity = repr(compiler+common).encode()+header_identity

def current(target, inputs):
    identity = hashlib.sha256(build_identity+b''.join(Path(p).read_bytes() for p in inputs)).hexdigest()
    stamp = Path(str(target)+'.sha256')
    return identity, stamp, target.exists() and stamp.exists() and stamp.read_text()==identity
def compile_core(source):
    target = objects/(Path(source).stem+'.o')
    identity, stamp, cached = current(target, [source])
    if not cached:
        subprocess.run(compiler+common+['-c',source,'-o',str(target)],cwd=ROOT,env=env,check=True)
        stamp.write_text(identity)
    return str(target)
with ThreadPoolExecutor(max_workers=3) as pool:
    core_objects = list(pool.map(compile_core,sources))
targets = [('rbtests', 'tests/CoreTests.cpp'), ('rbinspect', 'tools/Inspect.cpp')]
targets += [('rb'+name.lower(), 'tests/'+name+'Tests.cpp') for name in ('Motion', 'Protocol', 'Score', 'JudgeDispatch', 'Touch', 'CPU', 'Gameplay', 'AFP', 'MusicSheet', 'NoteFrame', 'Route', 'NoteGraphic', 'TouchInput', 'StageScoreInitialization', 'StageSession', 'AfpTimeline', 'StageGraphic', 'LongNoteGraphic', 'OriginalAfpMoviePool', 'GamePresentation', 'StageLongPresentation')]
for name, source in targets:
    binary = out/(name+('.exe' if sys.platform == 'win32' else ''))
    inputs = [ROOT/source, *core_objects, *sorted((ROOT/'tests').glob('*.hpp'))]
    identity, stamp, cached = current(binary, inputs)
    if not cached:
        subprocess.run(compiler+common+core_objects+[str(ROOT/source), '-o', str(binary)], cwd=ROOT, env=env, check=True)
        stamp.write_text(identity)
for name, source in targets:
    if source.startswith('tests/'):
        subprocess.run([str(out/(name+('.exe' if sys.platform == 'win32' else '')))], cwd=ROOT, check=True)
