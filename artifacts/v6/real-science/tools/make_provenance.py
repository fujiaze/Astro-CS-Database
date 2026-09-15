#!/usr/bin/env python3
"""REAL-SCIENCE-001 PROVENANCE 生成器（只读仓库 + 写 artifacts/v6/real-science/PROVENANCE.json）。"""
import hashlib, json, os, subprocess, sys

BASE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(BASE)))
ART = os.path.join(REPO, 'artifacts', 'v6', 'real-science')

def sha256(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for c in iter(lambda: f.read(1 << 20), b''):
            h.update(c)
    return h.hexdigest()

def cmd(c):
    return subprocess.run(c, cwd=REPO, shell=True, capture_output=True, text=True).stdout.strip()

head = cmd('git rev-parse HEAD')
version_file = open(os.path.join(REPO, 'VERSION')).read().strip()
binver = cmd('timeout 30 ./build/astrocs --version')

frozen_sources = [
    'lib/dynamic_psf/src/psf_information.cpp',
    'lib/snr_estimator/cpp/src/information_weight.cpp',
    'lib/photometric_calib/cpp/src/psfsw.cpp',
]
frozen_headers = [
    'lib/dynamic_psf/include/astrocs/v6/psf_information.h',
    'lib/snr_estimator/include/astrocs/v6/information_weight.h',
    'lib/photometric_calib/include/astrocs/v6/psfsw.h',
    'cli/v6_runtime_contract.h',
    'cli/v6_mode_gate.h',
]
contracts = [
    'docs/contracts/v6/frozen/01_DATA_CONTRACT_FREEZE.md',
    'docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md',
    'docs/science/v6/frozen/01_SEMANTIC_FREEZE.md',
    'docs/algorithms/v6/frozen/01_NUMERIC_THRESHOLD_FREEZE.md',
]

inv = {}
for root, _, files in os.walk(ART):
    for fn in sorted(files):
        fp = os.path.join(root, fn)
        inv[os.path.relpath(fp, REPO)] = {'sha256': sha256(fp), 'bytes': os.path.getsize(fp)}

prov = {
    'task': 'REAL-SCIENCE-001',
    'wave': 10,
    'baseline_head': head,
    'version_file': version_file,
    'cli_binary_version': binver,
    'driver_binary': 'run/v6/real-science/build/v6_real_science_driver',
    'negative_binary': 'run/v6/real-science/build/v6_real_science_negative',
    'frozen_kernel_sources': {f: sha256(os.path.join(REPO, f)) for f in frozen_sources},
    'frozen_headers': {f: sha256(os.path.join(REPO, f)) for f in frozen_headers},
    'frozen_contracts': {f: sha256(os.path.join(REPO, f)) for f in contracts},
    'resource_metrics': {
        'extract_M42_5frames': {'wall_s': 103.35, 'user_s': 48.91, 'sys_s': 53.89,
                                'maxrss_kb': 743596, 'fs_inputs_blocks': 8, 'fs_outputs_blocks': 456,
                                'source': 'run/v6/real-science/logs/08_extract_resource_probe.log'},
        'extract_all_9_datasets': {'wall_s_approx': 730,
                                   'evidence': 'inputs mtime 02:53:11 -> 03:05:21 (logs/01_extract_all.log)'},
        'driver_5modes_9datasets': {'wall_s': 0.280, 'user_s': 0.280, 'sys_s': 0.0, 'maxrss_kb': 6260,
                                    'fs_outputs_blocks': 744,
                                    'source': 'run/v6/real-science/logs/04_driver_run.log'},
        'oracle_numpy': {'wall_s': 0.21, 'user_s': 0.18, 'sys_s': 0.02, 'maxrss_kb': 30776,
                         'source': 'run/v6/real-science/logs/07_oracle.log'},
        'so05_resource_gate': 'PENDING_OWNER_SIGNOFF (记录 only, 见 C-007; 不得写成硬失败或 PASS 理由)',
    },
    'commands': [
        {'cmd': 'cmake --build build --target astrocs -j8', 'rc': 0, 'log': 'run/v6/real-science/logs/00_build_astrocs.log'},
        {'cmd': 'python3 run/v6/real-science/extract_real.py --out run/v6/real-science/inputs --frames 5', 'rc': 0, 'log': 'run/v6/real-science/logs/01_extract_all.log'},
        {'cmd': 'cmake -S run/v6/real-science/driver -B run/v6/real-science/build && cmake --build run/v6/real-science/build -j8', 'rc': 0, 'log': 'run/v6/real-science/logs/03_build_driver.log'},
        {'cmd': 'v6_real_science_driver --in run/v6/real-science/inputs --out run/v6/real-science/measurements/five_mode_measurements.json --csv ...csv', 'rc': 0, 'log': 'run/v6/real-science/logs/04_driver_run.log'},
        {'cmd': 'v6_real_science_negative', 'rc': 0, 'log': 'run/v6/real-science/logs/05_negative.jsonl'},
        {'cmd': 'bash run/v6/real-science/oracle/cli_mode_gate_check.sh', 'rc': 0, 'log': 'run/v6/real-science/logs/06_cli_mode_gate.jsonl'},
        {'cmd': 'python3 run/v6/real-science/oracle/v6_real_science_oracle.py --inputs ... --measurements ... --out ...', 'rc': 0, 'log': 'run/v6/real-science/logs/07_oracle.log'},
        {'cmd': 'python3 run/v6/real-science/summarize.py', 'rc': 0},
    ],
    'checks': {'oracle_checks': 486, 'oracle_fail': 0, 'negative_checks': 40, 'negative_fail': 0,
               'cli_mode_gate_fail': 0},
    'unavailable_or_unmeasured': [
        'psf_snr_power: DEFERRED (C-004.1)，未进入任何比较，也未运行。',
        '真实副产物 FITS 产品链 run_point_information/run_psfsw_robust 未在本任务运行（需先写 Phase1 磁盘产品）；W_info/PSFSW 权威量与 R C_in R^T 由冻结库函数直接计算并经独立 Oracle 复核。',
        '系统相关噪声（相关核）未估计：C_in 采用实测背景 RMS 的对角模型（无 GAIN/RDNOISE 头关键字）。',
        'SO-05 资源门为待负责人签字（record_only）。',
    ],
    'artifacts_inventory': inv,
}
out = os.path.join(ART, 'PROVENANCE.json')
json.dump(prov, open(out, 'w'), indent=2)
print('wrote', out, 'files_in_inventory=%d' % len(inv))
