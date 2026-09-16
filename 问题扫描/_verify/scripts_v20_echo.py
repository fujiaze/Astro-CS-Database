
import re, os, collections
src = open('lib/core/src/module_adapters.cpp', encoding='utf-8', errors='ignore').read()
wr = collections.Counter(re.findall(r'\(\*man\)\["([A-Za-z0-9_]+)"\]\s*=', src))
wr2 = collections.Counter(re.findall(r'\{"([A-Za-z0-9_]+)",', src))
keys = sorted(set(wr) | set(wr2))
# readers outside the writer file
def reads_of(k):
    hits = []
    for root in ['cli', 'lib', 'runtime', 'tests', 'tools']:
        for dp, dn, fn in os.walk(root):
            if 'third_party' in dp or '/build' in dp: continue
            for f in fn:
                if not f.endswith(('.c', '.cpp', '.h', '.py')): continue
                p = os.path.join(dp, f)
                if p.endswith('module_adapters.cpp'): continue
                t = open(p, encoding='utf-8', errors='ignore').read()
                for m in re.finditer(r'(?:value|contains|at)\(\s*"' + k + r'"|\["' + k + r'"\]', t):
                    ln = t[:m.start()].count('\n') + 1
                    hits.append('%s:%d' % (p, ln))
    return hits
zero = []
for k in keys:
    h = reads_of(k)
    if not h:
        zero.append(k)
print('manifest/artifact 写出键总数:', len(keys))
print('全仓无任何读者(除写出文件本身)的键数:', len(zero))
print('\n'.join('  ' + k for k in zero))
