#!/usr/bin/env python3
"""L4 重建：把 12 个 normalize 产出的 p1_products.json 合并为一个 49 产品数据集。

铁律：产品数必须 == 49（磁盘 R 帧数）。不足即判红，不得继续。
输出 mosaic 配置：output_dir 在顶层（与参考配置一致）。

§9.73 裁决 A44（WEIGHTMODE-CLEANUP-01）：**单一权重口径** —— 本配置**不写**任何权重键。
权重是阶段二按天球像素对应的输入帧集合现场算出的派生量 w = SNR²/F_ref² = 1/σ_F²，
没有可选择项（ASTROCS_DESIGN.md §3.1:171/175；docs/science/PSF_SIGNAL_WEIGHT.md §4:62/72）。
原 `weight_mode: 2` 与 `legacy_allow_weight_fallback` 两个键**已删除**，出现即配置解析失败。
"""
import json, sys, glob, os

REPO = '/workspace/Astro CS Database'
L4 = os.path.join(REPO, 'run/RELEASE-02/L4-rebuild')
EXPECTED = 49

def main():
    prods = sorted(glob.glob(os.path.join(L4, 'norm', '*', 'p1_products.json')))
    if not prods:
        print('FAIL: no p1_products.json found under', L4); return 2
    all_paths, per = [], {}
    for p in prods:
        d = json.load(open(p, encoding='utf-8'))
        tag = os.path.basename(os.path.dirname(p))
        hp = d.get('hips_paths', [])
        nf = d.get('n_frames'); npd = d.get('n_products'); cc = d.get('count_consistent')
        per[tag] = (nf, npd, len(hp), cc)
        if cc is not True:
            print('FAIL: %s count_consistent != true (%s)' % (tag, cc)); return 2
        if nf != npd or npd != len(hp):
            print('FAIL: %s n_frames=%s n_products=%s hips_paths=%s' % (tag, nf, npd, len(hp))); return 2
        for h in hp:
            all_paths.append(h if os.path.isabs(h) else os.path.join(os.path.dirname(p), h))
    total = len(all_paths)
    print('config  n_frames  n_products  hips_paths  count_consistent')
    for t in sorted(per):
        print('%-28s %3s %3s %3s %s' % ((t,)+per[t]))
    print('TOTAL products =', total, '(expected %d)' % EXPECTED)
    if total != EXPECTED:
        print('FAIL: product count != %d -- input not representative, DO NOT proceed' % EXPECTED); return 2
    missing = [p for p in all_paths if not os.path.isdir(p)]
    if missing:
        print('FAIL: %d product dirs missing, e.g. %s' % (len(missing), missing[0])); return 2
    cfg = {'schema_version': '1',
           'hips_paths': all_paths,
           'output_dir': os.path.join(L4, 'mosaic_out'),
           'algorithm_rejection_method': ''}
    out = os.path.join(L4, 'mosaic_49.json')
    json.dump(cfg, open(out,'w',encoding='utf-8'), ensure_ascii=False, indent=2)
    print('OK: wrote', out, 'with', total, 'paths, single weight path (no weight key)')
    return 0

if __name__ == '__main__':
    sys.exit(main())