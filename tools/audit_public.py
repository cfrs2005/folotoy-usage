#!/usr/bin/env python3
"""Audit allowlisted public source, Git's index and optional release artifacts."""
import argparse
import base64
from pathlib import Path
import json
import re
import subprocess
import sys
import zipfile
from package_source import FILES, TREES, SUFFIXES

ROOT = Path(__file__).resolve().parents[1]
PATTERNS = {
    'private key': rb'-----BEGIN (?:RSA |EC |OPENSSH |ENCRYPTED )?PRIVATE KEY-----(?:\r?\n|\\n)[A-Za-z0-9+/=\\\r\n]{64,}',
    'GitHub credential': rb'(?:gh[pousr]_|github_pat_)[A-Za-z0-9_]{24,}',
    'UsageHub credential': rb'uh_(?:display|enroll|ingest|api)_[A-Za-z0-9_-]{24,}',
    'AWS access key': rb'AKIA[0-9A-Z]{16}',
    'Google API key': rb'AIza[0-9A-Za-z_-]{35}',
    'personal home path': rb'/(?:Users|home)/[A-Za-z0-9._-]{2,}/',
}


def public_paths():
    result = [ROOT/name for name in FILES]
    for tree in TREES:
        result.extend(p for p in (ROOT/tree).rglob('*') if p.is_file() and p.suffix in SUFFIXES and '__pycache__' not in p.parts)
    return sorted(set(result))


def findings(data, sensitive):
    found = [name for name, pattern in PATTERNS.items() if re.search(pattern, data, re.IGNORECASE)]
    for value in sensitive:
        raw = value.encode()
        if raw in data or base64.b64encode(raw) in data or raw.hex().encode() in data:
            found.append('private release value')
            break
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--git', action='store_true', help='Also audit every indexed Git file')
    parser.add_argument('--artifact', type=Path, action='append', default=[])
    parser.add_argument('--sensitive-values-file', type=Path, help='Private JSON list of known sensitive strings; values are never printed')
    args = parser.parse_args()
    sensitive = json.loads(args.sensitive_values_file.read_text()) if args.sensitive_values_file else []
    if not isinstance(sensitive, list) or any(not isinstance(v, str) or len(v) < 4 for v in sensitive):
        raise ValueError('Sensitive values must be a list of strings of at least four characters')
    paths = public_paths(); allowed = {str(p.relative_to(ROOT)) for p in paths}; errors = []
    def check(name, data):
        for kind in findings(data, sensitive): errors.append(f'{name}: {kind}')
    for path in paths:
        name = str(path.relative_to(ROOT))
        if not path.is_file(): errors.append(f'{name}: missing public file'); continue
        if path.is_symlink() or any(parent.is_symlink() for parent in path.parents if parent != ROOT and ROOT in parent.parents):
            errors.append(f'{name}: symlink is not allowed'); continue
        check(name, path.read_bytes())
    if args.git:
        result = subprocess.run(['git', 'ls-files', '-z'], cwd=ROOT, check=True, capture_output=True)
        for raw_name in result.stdout.split(b'\0'):
            if not raw_name: continue
            name = raw_name.decode()
            if name not in allowed: errors.append(f'{name}: outside public source allowlist'); continue
            data = subprocess.run(['git', 'show', ':'+name], cwd=ROOT, check=True, capture_output=True).stdout
            check('index:'+name, data)
    for path in args.artifact:
        if zipfile.is_zipfile(path):
            with zipfile.ZipFile(path) as archive:
                for item in archive.infolist():
                    if item.is_dir(): continue
                    name = item.filename.removeprefix('folotoy-usage/')
                    if name not in allowed: errors.append(f'{path.name}:{name}: outside source allowlist')
                    check(path.name+':'+name, archive.read(item))
        else: check(path.name, path.read_bytes())
    if errors:
        print('\n'.join(sorted(set(errors))), file=sys.stderr)
        return 1
    print(f'Public audit: PASS ({len(paths)} source files, {len(args.artifact)} artifacts; sensitive values redacted)')
    return 0


if __name__ == '__main__': raise SystemExit(main())
