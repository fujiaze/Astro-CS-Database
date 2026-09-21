#!/usr/bin/env python3
"""Q2 stage D: parse p2_samples.json for bitref_16w and upmfix_out."""
import json, os, sys, numpy as np, collections

ROOT = '/workspace/Astro CS Database'
L4   = ROOT + '/run/RELEASE-02/L4-rebuild'
OUT  = ROOT + '/run/RELEASE-02/q2-snr-smooth/realdata'

def pct(v, qs=(0, 1, 16, 50, 84, 99, 100)):
    if v.size == 0: return None
    return {str(q): float(np.percentile(v, q)) for q in qs}

def field_stats(obs, key):
    raw = [o.get(key) for o in obs]
    n_none = sum(1 for x in raw if x is None)
    v = np.array([float(x) for x in raw if x is not None], dtype=np.float64)
    n = v.size
    out = dict(n=n, n_null=n_none,
               n_nan=int(np.isnan(v).sum()) if n else 0,
               n_zero=int((v == 0).sum()) if n else 0,
               n_nonzero=int((v != 0).sum()) if n else 0,
               n_pos=int((v > 0).sum()) if n else 0,
               n_neg=int((v < 0).sum()) if n else 0)
    if n:
        fv = v[np.isfinite(v)]
        if fv.size:
            out.update(min=float(fv.min()), max=float(fv.max()),
                       mean=float(fv.mean()), median=float(np.median(fv)),
                       percentile=pct(fv))
        else:
            out.update(min=None, max=None, mean=None, median=None, percentile=None)
    return out

report = {'schema': 'ASTROCS-Q2-REALDATA-P2SAMPLES', 'files': {}}

for tag, path in (('bitref_16w', L4 + '/bitref_16w/p2_samples.json'),
                  ('upmfix_out', L4 + '/upmfix_out/p2_samples.json')):
    print('=' * 100)
    print('PARSING', tag, path, os.path.getsize(path), 'bytes')
    with open(path) as f:
        J = json.load(f)
    print('  top-level keys:', sorted(J.keys()))
    controls = J['controls']; obs = J['observations']
    print('  controls[] entries:', len(controls), ' observations[] entries:', len(obs))
    print('  stats:', json.dumps(J['stats'], sort_keys=True))
    print('  target_order:', J.get('target_order'), ' control_grid_per_tile:', J.get('control_grid_per_tile'))

    cid = np.array([int(c['control_id']) for c in controls], dtype=np.int64)
    ocid = np.array([int(o['control_id']) for o in obs], dtype=np.int64)
    cnt = collections.Counter(ocid.tolist())
    cc = np.array([cnt.get(int(c), 0) for c in cid], dtype=np.int64)
    print('  unique control_id in controls[]:', int(np.unique(cid).size))
    print('  unique control_id in observations[]:', int(np.unique(ocid).size))
    print('  coverage-count (obs per control): min %d max %d mean %.3f median %.1f' % (
        cc.min(), cc.max(), cc.mean(), np.median(cc)))
    print('  controls with 0 obs: %d   with 1: %d   >=2: %d' % (
        int((cc == 0).sum()), int((cc == 1).sum()), int((cc >= 2).sum())))
    hist = collections.Counter(cc.tolist())
    print('  coverage-count histogram:', dict(sorted(hist.items())))
    # observation control_id not present in controls[]
    orphan = int(np.setdiff1d(np.unique(ocid), cid).size)
    print('  control_ids present in observations[] but NOT in controls[]:', orphan)

    fields = {}
    for k in ('ivar', 'snr', 'snr_available', 'uncertainty', 'value', 'support',
              'control_ivar', 'control_variance', 'quality_flags'):
        fields[k] = field_stats(obs, k)
        print('  %-17s' % k, json.dumps({kk: vv for kk, vv in fields[k].items() if kk != 'percentile'},
                                        sort_keys=True))
        print('  %-17s percentile %s' % ('', json.dumps(fields[k]['percentile'], sort_keys=True) if fields[k]['percentile'] else 'None'))

    # ---------------- per frame ----------------
    fr = collections.defaultdict(list)
    for i, o in enumerate(obs):
        fr[int(o['frame_id'])].append(i)
    print('  distinct frame_id:', len(fr))
    frames = {}
    for fid, idx in sorted(fr.items()):
        idx = np.array(idx, dtype=np.int64)
        val = np.array([float(obs[i]['value']) for i in idx], dtype=np.float64)
        cv  = np.array([float(obs[i]['control_variance']) for i in idx], dtype=np.float64)
        ci  = np.array([float(obs[i]['control_ivar']) for i in idx], dtype=np.float64)
        iv  = np.array([float(obs[i]['ivar']) for i in idx], dtype=np.float64)
        fin = np.isfinite(val)
        med = float(np.median(val[fin])) if fin.any() else None
        mad = float(1.4826 * np.median(np.abs(val[fin] - np.median(val[fin])))) if fin.any() else None
        frames[str(fid)] = dict(frame_id=int(fid), n_samples=int(idx.size),
                                value_median=med, value_mad=mad,
                                value_mean=float(np.nanmean(val)) if fin.any() else None,
                                value_min=float(np.nanmin(val)) if fin.any() else None,
                                value_max=float(np.nanmax(val)) if fin.any() else None,
                                control_variance_median=float(np.median(cv[np.isfinite(cv)])) if np.isfinite(cv).any() else None,
                                control_ivar_median=float(np.median(ci[np.isfinite(ci)])) if np.isfinite(ci).any() else None,
                                ivar_nonzero=int((iv != 0).sum()), snr_nonzero=int((np.array([float(obs[i]['snr']) for i in idx]) != 0).sum()))
    tot = sum(v['n_samples'] for v in frames.values())
    print('  per-frame sample counts: min %d max %d sum %d' % (
        min(v['n_samples'] for v in frames.values()), max(v['n_samples'] for v in frames.values()), tot))
    print('  per-frame value_median range: %.4g .. %.4g' % (
        min(v['value_median'] for v in frames.values()), max(v['value_median'] for v in frames.values())))
    print('  per-frame control_variance_median range: %.4g .. %.4g' % (
        min(v['control_variance_median'] for v in frames.values()),
        max(v['control_variance_median'] for v in frames.values())))
    nz_ivar = sum(v['ivar_nonzero'] for v in frames.values())
    nz_snr  = sum(v['snr_nonzero'] for v in frames.values())
    print('  TOTAL observations with ivar!=0: %d / %d   with snr!=0: %d / %d' % (nz_ivar, tot, nz_snr, tot))

    report['files'][tag] = dict(
        path=path, size_bytes=os.path.getsize(path),
        top_level_keys=sorted(J.keys()), stats=J['stats'],
        target_order=J.get('target_order'), control_grid_per_tile=J.get('control_grid_per_tile'),
        n_controls_entries=len(controls), n_observations=len(obs),
        n_unique_controls=int(np.unique(cid).size), n_unique_controls_in_obs=int(np.unique(ocid).size),
        controls_with_zero_obs=int((cc == 0).sum()),
        controls_with_one_obs=int((cc == 1).sum()),
        controls_with_ge2_obs=int((cc >= 2).sum()),
        coverage_count_stats=dict(min=int(cc.min()), max=int(cc.max()),
                                  mean=float(cc.mean()), median=float(np.median(cc)),
                                  histogram={str(k): int(v) for k, v in sorted(hist.items())}),
        orphan_control_ids_in_obs=orphan,
        field_stats=fields,
        frames=frames,
        n_frames=len(fr),
        total_samples=tot,
        n_obs_ivar_nonzero=int(nz_ivar), n_obs_snr_nonzero=int(nz_snr),
        per_control_coverage_counts=[[int(c), int(n)] for c, n in zip(cid.tolist(), cc.tolist())],
    )
    del J, controls, obs

with open(OUT + '/p2_samples_summary.json', 'w') as f:
    json.dump(report, f, indent=1)
print()
print('saved', OUT + '/p2_samples_summary.json')
