import pathlib, json, csv
p = pathlib.Path('docs/TRACEABILITY_MATRIX.csv')
print('EXISTS', p.exists())
raw = p.read_bytes()
print('FIRST BYTES', raw[:12])
print('HAS BOM', raw.startswith(b'\xef\xbb\xbf'))
j = pathlib.Path('docs/TRACEABILITY_MATRIX.json')
print('JSON EXISTS', j.exists())
if j.exists():
    d=json.loads(j.read_text(encoding='utf-8'))
    print('JSON TOPKEYS', list(d.keys())[:10] if isinstance(d,dict) else type(d))
import glob
print('matrix-like files:', glob.glob('docs/TRACEABILITY_MATRIX*')+glob.glob('docs/**/TRACEABILITY_MATRIX*', recursive=True))
