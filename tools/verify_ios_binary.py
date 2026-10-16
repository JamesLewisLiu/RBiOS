"""Fail CI if any app Mach-O declares a minimum iOS version above 14.0."""
import argparse
from pathlib import Path
import plistlib
import struct

def os_version(text):
    parts = tuple(map(int, text.split('.')))
    if not 1 <= len(parts) <= 3 or any(x < 0 for x in parts):
        raise ValueError('invalid minimum OS version')
    return parts + (0,) * (3 - len(parts))

def minimum_version(data, device_only=False):
    # Device product is arm64, thin or universal. Return each slice's minimum OS.
    magic = data[:4]
    if magic in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'):
        if len(data) < 8:raise ValueError('truncated universal header')
        count = struct.unpack_from('>I',data,4)[0]; wide = magic[-1]==191; out=[]
        if not count or count > (len(data)-8)//(32 if wide else 20):raise ValueError('invalid universal slice count')
        for i in range(count):
            at=8+i*(32 if wide else 20)
            off,size=struct.unpack_from('>QQ' if wide else '>II',data,at+8)
            if off > len(data) or size > len(data)-off:raise ValueError('universal slice exceeds file')
            versions = minimum_version(data[off:off+size], device_only)
            if not versions:raise ValueError('universal slice is not Mach-O')
            out.extend(versions)
        return out
    if magic not in (b'\xcf\xfa\xed\xfe',b'\xce\xfa\xed\xfe'):return []
    at=32 if magic[0]==207 else 28
    if len(data)<at:raise ValueError('truncated Mach-O header')
    if device_only and struct.unpack_from('<I',data,4)[0]!=0x100000c:raise ValueError('device binary is not arm64')
    ncmds,command_bytes=struct.unpack_from('<II',data,16)
    command_end=at+command_bytes
    if command_end>len(data) or ncmds>command_bytes//8:raise ValueError('invalid load command table')
    versions=[]
    for _ in range(ncmds):
        cmd,size=struct.unpack_from('<II',data,at)
        if size<8 or at+size>command_end:raise ValueError('invalid load command')
        if cmd==0x32:
            if size<24:raise ValueError('truncated build version command')
            platform,version=struct.unpack_from('<II',data,at+8)
            if platform not in (2,7):raise ValueError(f'non-iOS Mach-O platform {platform}')
            if device_only and platform!=2:raise ValueError('device archive contains simulator binary')
            versions.append(version)
        elif cmd==0x25:
            if size<16:raise ValueError('truncated minimum version command')
            versions.append(struct.unpack_from('<I',data,at+8)[0])
        at+=size
    if at!=command_end:raise ValueError('load command size mismatch')
    if not versions:raise ValueError('Mach-O missing minimum version')
    return versions

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('app',type=Path);args=p.parse_args()
    info=plistlib.loads((args.app/'Info.plist').read_bytes())
    if os_version(info['MinimumOSVersion'])>(14,0,0):raise SystemExit('Info.plist minimum OS exceeds 14.0')
    executable=args.app/info['CFBundleExecutable']
    if not executable.is_file() or not minimum_version(executable.read_bytes(), device_only=True):raise SystemExit('missing arm64 executable')
    found=0
    for file in args.app.rglob('*'):
        if file.is_file():
            versions=minimum_version(file.read_bytes(), device_only=True)
            for v in versions:
                if v>(14<<16):raise SystemExit(f'{file.name}: minimum OS {v>>16}.{(v>>8)&255} exceeds 14.0')
                found+=1
    if not found:raise SystemExit('no Mach-O verified')
    print(f'PASS: {found} Mach-O minimum-version records <= iOS 14.0')
