
import json, os, re, sys
cfg=json.load(open('ci/checks.json',encoding='utf-8'))
def walk(o, path=''):
    if isinstance(o, dict):
        for k,v in o.items(): yield from walk(v, path+'/'+str(k))
    elif isinstance(o, list):
        for i,v in enumerate(o): yield from walk(v, path+f'[{i}]')
    else: yield path, o
items = cfg if isinstance(cfg,list) else None
print('顶层类型:', type(cfg).__name__, '键:' , list(cfg.keys())[:12] if isinstance(cfg,dict) else len(cfg))
# find the list of checks
checks = None
if isinstance(cfg, dict):
    for k,v in cfg.items():
        if isinstance(v, list) and v and isinstance(v[0], dict) and ('id' in v[0] or 'name' in v[0]):
            checks=v; print('checks 列表键:', k, '长度', len(v)); break
if checks is None: checks=[]
import collections
targets = ['test_hips_input_contract','test_fits_stream_contract','hips_output_fixture','make_hips_fixture','test_healpix_io','test_psf_fit_handler','test_snr_estimator','hips_direct_smoke','v5_snr_precision_roundtrip','v5_maptile_oracle','hips_mapping_oracle','gen_hips_browser_test','diag_gaia_psf_projection','gate1_psf_final_test','gate2_psf_oracle','xpsd_cpp_crosscheck','xpsd_production_validation','tests/abi','abi']
print('\n### ci/checks.json 中与这些脚本相关的条目')
for c in checks:
    s=json.dumps(c, ensure_ascii=False)
    for t in targets:
        if t in s:
            print(f"  [{c.get('id', c.get('name'))}] 命中 '{t}'  profiles={c.get('profiles')}  waivable={c.get('waivable')}")
            m=re.search(r'"cmd[a-z]*"\s*:\s*(\[[^\]]*\]|"[^"]*")', s)
            print('     命令:', m.group(1)[:220] if m else s[:220])
            break
print('\n### 全部检查项 id 数:', len(checks))
ids=[c.get('id', c.get('name')) for c in checks]
print('### 含 ABI/abi 字样的检查项:', [i for i in ids if i and 'ABI' in str(i).upper()])
