"""Repair Homebrew runtime references left by macdeployqt and audit portability."""
import os
from pathlib import Path
import subprocess
import sys

contents = Path(sys.argv[1]).resolve() / 'Contents'
frameworks = contents / 'Frameworks'
changed = 0
for binary in contents.rglob('*'):
    if binary.is_symlink() or not binary.is_file():
        continue
    with binary.open('rb') as source:
        magic = source.read(4)
    if magic not in (b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe'):
        continue
    ids = subprocess.run(['otool', '-D', str(binary)], capture_output=True, text=True).stdout.splitlines()[1:]
    own_ids = {line.strip() for line in ids}
    deps = subprocess.check_output(['otool', '-L', str(binary)], text=True).splitlines()[1:]
    for line in deps:
        dep = line.strip().split(' (')[0]
        if dep in own_ids or not dep.startswith(('/opt/', '/Users/', '@rpath/')):
            continue
        if dep.startswith('@rpath/'):
            target = frameworks / dep[len('@rpath/'):]
        elif '.framework/' in dep:
            name, suffix = dep.split('.framework/', 1)
            target = frameworks / (Path(name).name + '.framework') / suffix
        else:
            target = frameworks / Path(dep).name
        if not target.exists() and binary.name == 'libqtposition_nmea.dylib':
            # Optional serial GPS plugin is unused by this desktop app.
            binary.unlink()
            break
        if not target.exists():
            raise RuntimeError(f'Missing bundled dependency: {binary}: {dep}')
        relative = '@loader_path/' + os.path.relpath(target, binary.parent)
        subprocess.run(['install_name_tool', '-change', dep, relative, str(binary)], check=True)
        changed += 1
print(f'Repaired {changed} external dependency references')
