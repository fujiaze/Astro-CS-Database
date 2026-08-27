
import os,re,json
root = 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/audit_stage2_32f.mosaic.hips/signal'
out = []
for dirpath, dirs, files in os.walk(root):
    for f in files:
        if not f.endswith('.fits'): continue
        rel = os.path.relpath(os.path.join(dirpath,f), root)
        m = re.match(r'Norder(\d+)/Dir(\d+)/Npix(\d+)\.fits$', rel.replace('\\','/'))
        if not m: continue
        K = int(m.group(1)); parent = int(m.group(3))
        out.append((K, parent, rel.replace('\\','/')))
print('COUNT', len(out))
with open('C:/Users/fujia/C_signal_list.json','w') as f:
    json.dump(out, f)
print('first', out[:3])
