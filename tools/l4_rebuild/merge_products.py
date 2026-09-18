#!/usr/bin/env python3
"""L4 重建：把 12 个 normalize 产出的 p1_products.json 合并为一个 49 产品数据集。

铁律：产品数必须 == 49（磁盘 R 帧数）。不足即判红，不得继续。
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
        nf = d.get('n_frames'); npd = d.get('n_products')
        cc = d.get('count_consistent')
        per[tag] = (nf, npd, len(hp), cc)
        if cc is not True:
            print('FAIL: %s count_consistent != true (%s)' % (tag, cc)); return 2
        if nf != npd or npd != len(hp):
            print('FAIL: %s n_frames=%s n_products=%s hips_paths=%s' % (tag, nf, npd, len(hp))); return 2
        for h in hp:
            all_paths.append(os.path.join(os.path.dirname(p), h) if not os.path.isabs(h) else h)
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
    cfg = {'hips_paths': all_paths, 'config': {'output_dir': os.path.join(L4,'mosaic_out')}}
    out = os.path.join(L4, 'mosaic_49.json')
    json.dump(cfg, open(out,'w',encoding='utf-8'), ensure_ascii=False, indent=2)
    print('OK: wrote', out, 'with', total, 'paths')
    return 0

if __name__ == '__main__':
    sys.exit(main())