
import re, collections
p='lib/core/src/module_adapters.cpp'
src=open(p,encoding='utf-8',errors='replace').read()
lines=src.split('\n')
pat = re.compile(r'(p1_int|p1_flag|p1_num|p1_has|\w+\.value)\s*\(\s*[A-Za-z_][\w>.\[\]-]*\s*,\s*"([a-z0-9_]+)"', re.M)
keys=collections.Counter(); sites=collections.defaultdict(list)
for i,l in enumerate(lines,1):
    for m in pat.finditer(l):
        keys[m.group(2)]+=1; sites[m.group(2)].append(i)
print("distinct parsed keys:", len(keys), " total sites:", sum(keys.values()))
for k,c in sorted(keys.items()):
    print(f"{k}\t{c}\t{sites[k][:10]}")
