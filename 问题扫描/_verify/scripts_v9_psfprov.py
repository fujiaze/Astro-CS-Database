import subprocess, csv, io
def blob(rev,path):
    return subprocess.run(['git','--no-optional-locks','show',rev+':'+path],capture_output=True,text=True).stdout
for rev in ('3dbf4723','21506782','535e7387','c3452d48','HEAD'):
    t=blob(rev,'docs/TRACEABILITY.csv')
    if not t: print(rev,'NO BLOB'); continue
    rows=list(csv.DictReader(io.StringIO(t)))
    ids=';'.join(r.get('requirement_id','') for r in rows)
    psf=[r['requirement_id'] for r in rows if 'PSF' in r.get('requirement_id','').upper()]
    rej=[r['requirement_id'] for r in rows if 'REJ' in r.get('requirement_id','').upper()]
    print('%-10s rows=%-4d PSF-ids=%-2d REJ-ids=%-2d  %s %s' % (rev,len(rows),len(psf),len(rej),psf[:3],rej[:3]))
print()
t=blob('HEAD','docs/TRACEABILITY.csv')
rows=list(csv.DictReader(io.StringIO(t)))
print('HEAD rows:', len(rows))
print('any id containing PSF anywhere in row text:')
for r in rows:
    s=' '.join(str(v) for v in r.values())
    if 'PSF' in s.upper(): print('   ', r.get('requirement_id'), '|', s[:120])
print('rows mentioning REJ in any column:')
for r in rows:
    s=' '.join(str(v) for v in r.values())
    if 'REJ' in s.upper(): print('   ', r.get('requirement_id'), '|', s[:120])
