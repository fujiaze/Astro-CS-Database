
import re
lines=open('lib/snr_estimator/src/module_entry.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('--- plan 765-776 ---')
print('\n'.join('%d: %s' % (i+1, lines[i].rstrip()[:120]) for i in range(764,776)))
print()
for f in ['cfg_present','variance_floor_given','variance_given','signal_given','gain_given','alpha_given','ivar_given','variance_floor']:
    sites=[(i+1, lines[i].strip()[:100]) for i,l in enumerate(lines) if re.search(r'(?:->|\.)\s*'+f+r'\b', l)]
    w=[n for n,s in sites if re.search(r'(?:->|\.)\s*'+f+r'\b\s*=[^=]', s)]
    rd=[(n,s) for n,s in sites if n not in w]
    print('%-22s total=%d write=%s read=%d %s' % (f, len(sites), w, len(rd), rd[:2]))
