
import re
lines=open('lib/snr_estimator/cpp/src/snr_estimator.cpp', encoding='utf-8').read().split('\n')
# build map line -> enclosing SNR_API function
cur=None; fn={}
for i,l in enumerate(lines, start=1):
    m=re.match(r'SNR_API\s+\w+\s+(\w+)\s*\(', l)
    if m: cur=m.group(1)
    fn[i]=cur
callers=[i for i,l in enumerate(lines, start=1) if 'sourceSnrFromPsfRow(' in l and 'inline double' not in l]
print('sourceSnrFromPsfRow call sites:', callers)
import collections
c=collections.Counter(fn[i] for i in callers)
print('enclosing exported functions:', dict(c), 'distinct =', len(c))
direct=[i for i,l in enumerate(lines, start=1) if 'snr_source_snr_f64' in l]
print('snr_source_snr_f64 lines:', direct, [fn[i] for i in direct])
