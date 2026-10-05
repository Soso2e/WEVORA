"""Compare tracked logical bytes (LFS payload sizes), without counting Git history/DDC."""
import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def git(*args, **kwargs):
    return subprocess.check_output(['git', '-C', str(ROOT), *args], **kwargs)


def snapshot(ref=None):
    files = git('ls-tree', '-r', '--name-only', '-z', ref) if ref else git('ls-files', '-z')
    paths = [p for p in files.decode().split('\0') if p]
    if ref:
        names = ''.join(f'{ref}:{path}\n' for path in paths).encode()
        raw = git('cat-file', '--batch', input=names)
        blobs = {}
        offset = 0
        for path in paths:
            end = raw.index(b'\n', offset)
            size = int(raw[offset:end].split()[-1])
            offset = end + 1
            blobs[path] = raw[offset:offset + size]
            offset += size + 1
    else:
        blobs = {path: (ROOT / path).read_bytes() for path in paths}
    sizes = {}
    for path, data in blobs.items():
        if data.startswith(b'version https://git-lfs.github.com/spec/v1\n'):
            sizes[path] = int(re.search(rb'^size (\d+)$', data, re.M)[1])
        else:
            sizes[path] = len(data)
    variants = [p for p in paths if any(part.startswith('Variant_') for part in Path(p).parts)]
    project = json.loads(blobs['WEVORA.uproject'])
    build = blobs['Source/WEVORA/WEVORA.Build.cs'].decode()
    build = re.sub(r'/\*.*?\*/|//[^\n]*', '', build, flags=re.S)
    dependencies = []
    for match in re.finditer(r'(?:Public|Private)DependencyModuleNames.AddRange\(new string\[\]\s*{(.*?)}\)', build, re.S):
        dependencies.extend(re.findall(r'"([\w]+)"', match[1]))
    return {
        'tracked_files': len(paths),
        'variant_files': len(variants),
        'variant_bytes': sum(sizes[p] for p in variants),
        'cpp_files': sum(p.endswith('.cpp') for p in paths),
        'cpp_headers': sum(p.endswith('.h') for p in paths),
        'explicit_enabled_plugins': [p['Name'] for p in project['Plugins'] if p['Enabled']],
        'module_dependencies': dependencies,
        'tracked_logical_bytes': sum(sizes.values()),
        'content_logical_bytes': sum(size for p, size in sizes.items() if p.startswith('Content/')),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', default='origin/main')
    args = parser.parse_args()
    before, after = snapshot(args.base), snapshot()
    print(json.dumps({'base': args.base, 'before': before, 'after': after,
                      'variant_files_removed': before['variant_files'] - after['variant_files']}, indent=2))


if __name__ == '__main__':
    main()
