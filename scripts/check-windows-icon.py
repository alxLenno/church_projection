"""Verify native Windows icon resources in a built executable."""
from pathlib import Path
import struct
import sys

data = Path(sys.argv[1]).read_bytes()
pe = struct.unpack_from('<I', data, 60)[0]
assert data[pe:pe + 4] == b'PE\0\0'
optional = pe + 24
magic = struct.unpack_from('<H', data, optional)[0]
directories = optional + (112 if magic == 0x20b else 96)
resource_rva, resource_size = struct.unpack_from('<II', data, directories + 16)
assert resource_rva and resource_size, 'Executable has no resource table'
section_count = struct.unpack_from('<H', data, pe + 6)[0]
section_base = optional + struct.unpack_from('<H', data, pe + 20)[0]
for index in range(section_count):
    section = section_base + index * 40
    virtual_size, rva, raw_size, raw = struct.unpack_from('<IIII', data, section + 8)
    if rva <= resource_rva < rva + max(virtual_size, raw_size):
        root = raw + resource_rva - rva
        break
else:
    raise AssertionError('Resource section not found')
named, numbered = struct.unpack_from('<HH', data, root + 12)
types = {struct.unpack_from('<I', data, root + 16 + i * 8)[0] for i in range(named + numbered)}
assert 3 in types, 'Missing native RT_ICON resource'
assert 14 in types, 'Missing native RT_GROUP_ICON resource'
print('PASS: native Windows application icon is embedded')
