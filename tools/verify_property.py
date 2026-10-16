"""Independent offline KBinXML wire comparison, not a real-server compatibility test."""
import hashlib
import json
from pathlib import Path
import random
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.tools'))
from kbinxml import KBinXML
from kbinxml.format_ids import xml_formats
from lxml import etree

exe = ROOT / '.build' / ('rbinspect.exe' if sys.platform == 'win32' else 'rbinspect')
rng = random.Random(2019121100)
fixtures = []

def field(parent, index, type_id, count=None):
    fmt = xml_formats[type_id]
    node = etree.SubElement(parent, f'field{index}', __type=fmt['name'])
    if type_id == 1:
        return
    if type_id == 10:
        node.text = '001122ff7f80'
    elif type_id == 11:
        node.text = '\u539f\u7248\u30c6\u30b9\u30c8'
    elif type_id == 12:
        node.text = '192.0.2.3'
    else:
        values = []
        for _ in range(fmt['count'] * (count or 1)):
            if fmt['type'] in ('f', 'd'):
                values.append(str(rng.choice((-1.5, 0.25, 2.0))))
            else:
                values.append(str(rng.randint(0, 100)))
        node.text = ' '.join(values)
    if count is not None:
        node.set('__count', str(count))
    if index % 3 == 0:
        node.set('status', '0')
        node.set('name', '\u6587\u5b57\u5217')

all_types = [x for x in xml_formats if x != 46]
tree = etree.Element('response', model='MBR')
for i, type_id in enumerate(all_types):
    field(tree, i, type_id)
fixtures.append(tree)
for _ in range(12):
    tree = etree.Element('response', status='0')
    group = etree.SubElement(tree, 'synthetic')
    for i in range(60):
        type_id = rng.choice(all_types)
        count = rng.randint(1, 5) if type_id not in (1, 10, 11, 12) and rng.choice((True, False)) else None
        field(group, i, type_id, count)
    fixtures.append(tree)

cases = []
for index, tree in enumerate(fixtures):
    reference = KBinXML(tree)
    for encoding in ('cp932', 'UTF-8'):
        expected = reference.to_binary(encoding=encoding, compressed=True)
        for packed in (False, True):
            source = reference.to_binary(encoding=encoding, compressed=packed)
            actual = subprocess.check_output([str(exe), '--property-roundtrip'], input=source)
            if actual != expected:
                raise AssertionError((index, encoding, packed, 'wire bytes differ'))
            cases.append({'fixture': index, 'encoding': encoding, 'input_packed': packed,
                          'bytes': len(actual), 'sha256': hashlib.sha256(actual).hexdigest()})
report = {'cases': len(cases), 'types': all_types, 'transport': 'none',
          'limitations': ['synthetic property trees, not original EA3 envelopes or business messages'],
          'fixtures': cases}
(ROOT / '.build/property-parity.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(f'PASS {len(cases)} KBinXML comparisons against kbinxml 2.2 (no transport)')
