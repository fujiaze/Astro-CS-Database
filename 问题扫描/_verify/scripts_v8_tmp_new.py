
import json
added = json.load(open('问题扫描/_verify/_v8_added.json', encoding='utf-8'))
checks = [('lib/snr_estimator/cpp/include/snr_estimator.h', [209,212,215,217,218,219,220]),
          ('lib/phase1/noise/snr_frame_science.cpp', [3,4,5,6,7,8,30,86,155]),
          ('lib/snr_estimator/README.md', [212,213,214]),
          ('lib/core/src/runtime.cpp', [102,103,104]),
          ('lib/star_detector/src/sdet_angle_guard.h', [39]),
          ('lib/gaia_xpsd_client/src/gaia_client.h', [3])]
for fp, lns in checks:
    have = {a['n'] for a in added if a['f']==fp}
    print(fp, {ln: (ln in have) for ln in lns})
