import pathlib
p=pathlib.Path('docs/traceability/TRACEABILITY_MATRIX.csv')
b=p.read_bytes()
print('SIZE', len(b), 'FIRST12', b[:12])
print('HAS_BOM', b.startswith(b'\xef\xbb\xbf'))
lines=b.decode('utf-8-sig').splitlines()
print('LINES', len(lines))
print('HEADER', lines[0])
