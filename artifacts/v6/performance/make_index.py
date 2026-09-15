import hashlib, json, pathlib
ART = pathlib.Path('artifacts/v6/performance')
files = {}
for p in sorted(ART.rglob('*')):
    if p.is_file():
        h = hashlib.sha256(p.read_bytes()).hexdigest()
        files[str(p.relative_to(ART)).replace('\\','/')] = {'sha256': h, 'bytes': p.stat().st_size}
doc = {'schema': 'astrocs.v6.perf.artifacts-index/v1',
       'root': 'artifacts/v6/performance',
       'n_files': len(files), 'files': files}
(ART / 'ARTIFACTS_INDEX.json').write_text(json.dumps(doc, ensure_ascii=False, indent=2) + '\n')
print('indexed', len(files), 'files')
for k in files: print('  ', k)
