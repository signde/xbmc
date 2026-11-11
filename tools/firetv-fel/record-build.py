#!/usr/bin/env python3
"""Record the source and dependency inputs for a Kodi FEL candidate."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import zipfile

root = Path(__file__).resolve().parents[2]
apk = Path(sys.argv[1]).resolve()
workspace = root.parent

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def git(*args):
    return subprocess.check_output(['git', '-C', str(root), *args])

changed = set(git('diff', '--name-only', 'HEAD', '-z').decode().split('\0'))
added = set(git('ls-files', '--others', '--exclude-standard', '-z').decode().split('\0'))
files = {p: sha(root/p) if (root/p).is_file() else None
         for p in sorted((changed | added) - {''})}
patch = Path(str(apk)+'.source.patch')
patch.write_bytes(git('diff', '--binary', 'HEAD'))
with tarfile.open(str(apk)+'.added-source.tar.gz', 'w:gz') as tar:
    for p in sorted(added - {''}):
        if (root/p).is_file():
            tar.add(root/p, arcname=p, recursive=False)
inputs = {}
for base in ('libdvfelplay', 'libdovi3-arm'):
    for path in sorted((workspace/base).rglob('*')):
        if path.is_file() and not any(part.startswith('.') for part in path.relative_to(workspace/base).parts):
            inputs[str(path.relative_to(workspace))] = sha(path)
with zipfile.ZipFile(apk) as z:
    elf = z.read('lib/armeabi-v7a/libdvfelplay.so')
    assert elf[:5] == b'\x7fELF\x01', 'FEL library must be ARM32 ELF'
    assert b'dvfel_module_blob_start' not in elf, 'Embedded kernel payload is not allowed'
    expected = workspace/'kodi-build/tools/android/packaging/xbmc/lib/armeabi-v7a/libdvfelplay.so'
    assert elf == expected.read_bytes(), 'APK FEL library differs from packaged build input'
    native = {n: hashlib.sha256(z.read(n)).hexdigest() for n in z.namelist() if n.startswith('lib/') and n.endswith('.so')}
with tempfile.TemporaryDirectory() as tmp:
    out = Path(tmp)/'elf'
    out.write_bytes(elf)
    readelf = workspace/'sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf'
    dynamic = subprocess.check_output([str(readelf), '-d', str(out)], text=True)
    assert 'libc++_shared.so' not in dynamic, 'FEL library needs a static C++ runtime'
manifest = {
    'kodi_commit': git('rev-parse', 'HEAD').decode().strip(),
    'source_files': files,
    'patch_sha256': sha(patch),
    'inputs': inputs,
    'apk_sha256': sha(apk),
    'native_libraries': native,
    'toolchain': {'ndk': '28.2.13676358', 'sdk': '37', 'build_tools': '36.0.0', 'abi': 'armeabi-v7a', 'api': 26},
}
Path(str(apk)+'.build.json').write_text(json.dumps(manifest, indent=2, sort_keys=True)+'\n')
print('Verified ARM32 FEL payload and recorded source/build inputs')
