
import json
added = json.load(open('问题扫描/_verify/_v8_added.json', encoding='utf-8'))
for fp in ['lib/snr_estimator/cpp/include/snr_estimator.h','lib/phase1/noise/snr_frame_science.h','lib/core/src/runtime.cpp']:
    ns=[a['n'] for a in added if a['f']==fp]
    print(fp, 'added lines:', sorted(ns)[:40])
';
'
