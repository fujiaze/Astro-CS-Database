import sys, os, re
sys.stdout.reconfigure(encoding='utf-8')
root = r"F:/Astro dev/Astro CS Normalization Database"
raw=open(os.path.join(root,"eng/packaging/config/filters.json"),encoding="utf-8").read().splitlines()
# print line numbers of 2-space-indented keys (filter names) after 'filters'
start=None
for i,l in enumerate(raw,1):
    if l.strip()=='"filters": {' and i>800: start=i; break
print("filters block starts at", start)
for i in range(start, min(start+900,len(raw))):
    l=raw[i-1]
    if re.match(r'^    "[^"]+": \{', l) or re.match(r'^      "(channel|wavelength_nm|value|n_points|name)"', l):
        print(i, l[:70])
