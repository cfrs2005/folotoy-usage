#!/usr/bin/env python3
"""Export only the standalone public source tree; never walk the parent project."""
from pathlib import Path
import hashlib
import zipfile
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FILES = ['CMakeLists.txt', 'sdkconfig.defaults', 'partitions.csv', 'dependencies.lock',
         'LICENSE', 'README.md', 'README.zh_CN.md', 'SECURITY.md', 'CONTRIBUTING.md', '.gitignore']
TREES = ['main', 'components/bsp', 'bootloader_components/recovery_boot_hook', 'tests', 'tools', 'assets/fonts', 'docs', '.github/workflows']
SUFFIXES = {'.c', '.h', '.py', '.sh', '.yml', '.txt', '.md'}


def main():
    subprocess.run([sys.executable, str(ROOT / "tools/audit_public.py")], check=True)
    paths = [ROOT / name for name in FILES]
    for name in TREES:
        paths.extend(p for p in (ROOT / name).rglob('*') if p.is_file() and p.suffix in SUFFIXES and '__pycache__' not in p.parts)
    output = ROOT / 'dist'; output.mkdir(exist_ok=True)
    archive = output / 'folotoy-usage-source.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for path in sorted(set(paths)):
            if path.is_symlink(): raise ValueError('Symlinks are not allowed in source exports')
            z.write(path, 'folotoy-usage/' + str(path.relative_to(ROOT)))
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (output / 'SHA256SUMS').write_text(f'{digest}  {archive.name}\n')
    print(f'Source export: {archive}')


if __name__ == '__main__': main()
