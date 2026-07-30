# -*- coding: utf-8 -*-
"""run_e004.py - 运行 E-004 注入恢复测试, 生成 JSON 报告."""
from __future__ import annotations
import json
import os
import sys

PROJECT = r'f:\Astro dev\Astro CS Normalization Database'
os.chdir(PROJECT)
sys.path.insert(0, os.path.join('lib', 'healpix_db', 'healpix_stack', 'python', 'e_chain'))
sys.path.insert(0, os.path.join('lib', 'astro_image_io', 'python'))

from e_common import load_all_panels, setup_logger
from e_injection_test import run_all_tests

log_dir = os.path.join('lib', 'healpix_db', 'healpix_stack', 'python', 'e_chain', 'logs')
evid_dir = os.path.join('engineering_authoritative', 'evidence', 'E-004')
os.makedirs(log_dir, exist_ok=True)
os.makedirs(evid_dir, exist_ok=True)

log = setup_logger('e004_run', log_dir=log_dir)
panels = load_all_panels()
report = run_all_tests(panels, logger=log)

with open(os.path.join(evid_dir, 'e004_report.json'), 'w', encoding='utf-8') as f:
    json.dump(report, f, indent=2, default=str, ensure_ascii=False)

print()
print('=== SUMMARY ===')
tA = report['tests']['A']
tB = report['tests']['B']
tC = report['tests']['C']
tD = report['tests']['D']
print('A (gradient recovery): PASS=%s' % tA['pass_overall'])
print('B (outlier masking):   PASS=%s' % tB['pass_b'])
print('C (SNR weight):        PASS=%s' % tC['pass_c'])
print('D (joint recovery):    PASS=%s' % tD['pass_d'])
print('ALL PASS: %s' % report['all_pass'])
if not report['all_pass']:
    print()
    print('=== FAILURE DETAILS ===')
    if not tA['pass_overall']:
        print('A: a_err=%.4f%% b_err=%.4f%%' % (
            tA['error']['a_ra_rel'] * 100, tA['error']['b_dec_rel'] * 100))
    if not tD['pass_d']:
        print('D: a_err=%.4f%% b_err=%.4f%%' % (
            tD['error']['a_ra_rel'] * 100, tD['error']['b_dec_rel'] * 100))
