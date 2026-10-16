import importlib.util
from pathlib import Path
import struct
import unittest

spec=importlib.util.spec_from_file_location('verify',Path(__file__).resolve().parents[1]/'tools/verify_ios_binary.py')
verify=importlib.util.module_from_spec(spec);spec.loader.exec_module(verify)

class MinimumVersionTests(unittest.TestCase):
    def test_version_padding(self):
        self.assertEqual(verify.os_version('14.0.0'),verify.os_version('14.0'))
        self.assertGreater(verify.os_version('14.1'),verify.os_version('14.0'))
    def test_arm64_build_version(self):
        data=struct.pack('<8I',0xfeedfacf,0x100000c,0,2,1,24,0,0)+struct.pack('<6I',0x32,24,2,14<<16,18<<16,0)
        self.assertEqual(verify.minimum_version(data),[14<<16])
    def test_old_load_command(self):
        data=struct.pack('<8I',0xfeedfacf,0x100000c,0,2,1,16,0,0)+struct.pack('<4I',0x25,16,14<<16,17<<16)
        self.assertEqual(verify.minimum_version(data),[14<<16])
    def test_non_macho(self):self.assertEqual(verify.minimum_version(b'plain text'),[])
    def test_wrong_platform_rejected(self):
        data=struct.pack('<8I',0xfeedfacf,0x100000c,0,2,1,24,0,0)+struct.pack('<6I',0x32,24,1,14<<16,18<<16,0)
        with self.assertRaises(ValueError):verify.minimum_version(data)
    def test_simulator_rejected_for_device(self):
        data=struct.pack('<8I',0xfeedfacf,0x100000c,0,2,1,24,0,0)+struct.pack('<6I',0x32,24,7,14<<16,18<<16,0)
        self.assertEqual(verify.minimum_version(data),[14<<16])
        with self.assertRaises(ValueError):verify.minimum_version(data,device_only=True)
    def test_truncated_command(self):
        data=struct.pack('<8I',0xfeedfacf,0x100000c,0,2,1,24,0,0)+struct.pack('<3I',0x32,24,2)
        with self.assertRaises(ValueError):verify.minimum_version(data)
    def test_universal_out_of_bounds(self):
        data=struct.pack('>7I',0xcafebabe,1,0x100000c,0,4096,100,0)
        with self.assertRaises(ValueError):verify.minimum_version(data)
    def test_universal_slice(self):
        thin=struct.pack('<8I',0xfeedfacf,0x100000c,0,2,1,16,0,0)+struct.pack('<4I',0x25,16,14<<16,17<<16)
        fat=struct.pack('>7I',0xcafebabe,1,0x100000c,0,28,len(thin),0)+thin
        self.assertEqual(verify.minimum_version(fat),[14<<16])

if __name__=='__main__':unittest.main()
