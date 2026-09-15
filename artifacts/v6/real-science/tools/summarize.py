#!/usr/bin/env python3
"""REAL-SCIENCE-001 汇总器：从五点测量 JSON 生成 summary_metrics.csv 与 input_manifest.json。"""
import json, hashlib, os, sys

BASE = os.path.dirname(os.path.abspath(__file__))
M = os.path.join(BASE, 'measurements')
d = json.load(open(os.path.join(M, 'five_mode_measurements.json')))
modes = ['equal', 'exposure', 'ivar', 'w_info', 'psfsw']

def med(xs):
    xs = sorted(xs); n = len(xs)
    return xs[n // 2] if n % 2 else 0.5 * (xs[n // 2 - 1] + xs[n // 2])

cols = ['dataset','object','telescope','n_frames','n_stars','exptimes_s','global_bg_median_ADU',
        'global_bg_rms_ADU'] + ['median_native_snr_' + m for m in modes] + \
       ['median_mf_snr_' + m for m in modes] + ['fwhm_' + m + '_px' for m in modes] + \
       ['native_var_ratio_w_info_over_equal','native_var_ratio_ivar_over_equal',
        'mf_var_ratio_w_info_over_equal','mf_snr_ratio_w_info_over_equal',
        'a_nea_mean_px2','frame_coverage','psfsw_defined_fraction','psfsw_median_w']
rows = [','.join(cols)]
for ds in d['datasets']:
    nat = [med([p['modes'][m]['snr'] for p in ds['per_star']]) for m in modes]
    mf = [med([p['modes'][m]['mf_snr'] for p in ds['per_star']]) for m in modes]
    s0 = ds['per_star'][0]['modes']
    wp = sorted(ds['group_psfsw']['w_psfsw'])
    vals = [ds['dataset'], ds['object'], ds['telescope'], ds['n_frames'], ds['n_stars'],
            '|'.join(str(x) for x in sorted(set(ds['exptimes']))),
            '%.4f' % ds['global_bg_median_ADU'], '%.4f' % ds['global_bg_rms_ADU']]
    vals += ['%.6g' % x for x in nat] + ['%.6g' % x for x in mf]
    vals += ['%.4f' % s0[m]['effective_psf_fwhm_px'] for m in modes]
    vals += ['%.6f' % (s0['w_info']['var_ADU2'] / s0['equal']['var_ADU2']),
             '%.6f' % (s0['ivar']['var_ADU2'] / s0['equal']['var_ADU2']),
             '%.6f' % (s0['w_info']['mf_var_ADU2'] / s0['equal']['mf_var_ADU2']),
             '%.6f' % (mf[3] / mf[0]),
             '%.6f' % ds['coverage']['a_nea_mean_px2'], '%.4f' % ds['coverage']['frame_coverage'],
             '%.4f' % ds['coverage']['psfsw_defined_fraction'], '%.6f' % wp[len(wp) // 2]]
    rows.append(','.join(str(v) for v in vals))
open(os.path.join(M, 'summary_metrics.csv'), 'w').write('\n'.join(rows) + '\n')
print('summary_metrics.csv rows=%d' % (len(rows) - 1))

man = {'task': 'REAL-SCIENCE-001', 'datasets': []}
for ds in d['datasets']:
    fn = os.path.join(BASE, 'inputs', ds['dataset'] + '.json')
    h = hashlib.sha256(open(fn, 'rb').read()).hexdigest()
    man['datasets'].append({'dataset': ds['dataset'], 'object': ds['object'], 'telescope': ds['telescope'],
                            'input_json': os.path.relpath(fn, os.path.dirname(BASE)),
                            'input_json_sha256': h, 'n_frames': ds['n_frames'], 'n_stars': ds['n_stars'],
                            'frame_paths': ds['frame_paths'], 'frame_sha256': ds['frame_sha256'],
                            'exptimes': ds['exptimes']})
json.dump(man, open(os.path.join(M, 'input_manifest.json'), 'w'), indent=2)
print('input_manifest.json datasets=%d frames=%d' % (len(man['datasets']),
      sum(len(x['frame_sha256']) for x in man['datasets'])))
