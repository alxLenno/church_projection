"""Keep only the requested CPU architecture in a deployed macOS bundle."""
from pathlib import Path
import subprocess
import sys

bundle = Path(sys.argv[1])
architecture = sys.argv[2] if len(sys.argv) > 2 else 'arm64'
count = 0
for binary in bundle.rglob('*'):
    if not binary.is_file() or binary.is_symlink():
        continue
    with binary.open('rb') as source:
        magic = source.read(4)
    if magic not in (b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'):
        continue
    arches = subprocess.check_output(['lipo', '-archs', str(binary)], text=True).split()
    if architecture not in arches:
        raise RuntimeError(f'{binary.name} does not support {architecture}')
    if len(arches) > 1:
        output = binary.with_name(binary.name + '.thin')
        subprocess.run(['lipo', str(binary), '-thin', architecture, '-output', str(output)], check=True)
        output.chmod(binary.stat().st_mode)
        output.replace(binary)
        count += 1
print(f'Removed unused architectures from {count} binaries; kept {architecture}')
