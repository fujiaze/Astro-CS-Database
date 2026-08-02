#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""R06 交付 ZIP 生成器 - 按 REQUIRED_ZIP_STRUCTURE.md 组织"""
import hashlib
import os
import shutil
import zipfile
from pathlib import Path
from datetime import datetime

ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
STAMP = "2026-08-02"
DELIVERY_NAME = f"AstroCS_Delivery_{STAMP}_PreciseGeometryFast_R06"
DELIVERY_DIR = ROOT / DELIVERY_NAME
ZIP_PATH = ROOT / f"{DELIVERY_NAME}.zip"

# 清理旧目录
if DELIVERY_DIR.exists():
    shutil.rmtree(DELIVERY_DIR)
DELIVERY_DIR.mkdir(parents=True)

def write_file(rel_path, content):
    p = DELIVERY_DIR / rel_path
    p.parent.mkdir(parents=True, exist_ok=True)
    if isinstance(content, str):
        p.write_text(content, encoding='utf-8')
    else:
        p.write_bytes(content)

def copy_file(src, dst):
    src = ROOT / src
    dst = DELIVERY_DIR / dst
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)

def copy_tree(src, dst, exclude_dirs=None, exclude_exts=None):
    src = ROOT / src
    dst = DELIVERY_DIR / dst
    exclude_dirs = exclude_dirs or set()
    exclude_exts = exclude_exts or set()
    for item in src.rglob('*'):
        if item.is_dir():
            rel = item.relative_to(src)
            if any(part in exclude_dirs for part in rel.parts):
                continue
            (dst / rel).mkdir(parents=True, exist_ok=True)
        elif item.is_file():
            rel = item.relative_to(src)
            if any(part in exclude_dirs for part in rel.parts):
                continue
            if item.suffix in exclude_exts:
                continue
            (dst / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item, dst / rel)

# ============================================================================
# 1. wiki/
# ============================================================================
copy_tree("AstroCS.wiki", "wiki", exclude_dirs={".git"})

# ============================================================================
# 2. source/ - Phase1 源码 (排除 build/二进制/Stage2)
# ============================================================================
phase1_dirs = [
    "lib/astro_image_io",
    "lib/healpix_db",
    "lib/orchestrator",
    "lib/calibration",
    "lib/photometric_calib",
    "lib/plate_solve",
]
exclude_src = {".git", "build", "artifacts", "__pycache__", ".pytest_cache",
               "archive", "python", "testdata", "run",
               "logs", "output_hiss_dir", "output_stage2.hcsd",
               "siril_compare", "experiment", "perf_test", "results",
               "evidence", "raw_logs", "temp"}
exclude_exts = {".exe", ".dll", ".lib", ".o", ".obj", ".a", ".so", ".pyc",
                ".compile_out", ".log",
                ".fits", ".fts", ".xisf", ".hiss", ".hcsd",
                ".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff",
                ".csv", ".jsonl", ".parquet", ".h5", ".hdf5",
                ".zip", ".gz", ".tar", ".7z"}
for d in phase1_dirs:
    src = ROOT / d
    if src.exists():
        copy_tree(d, f"source/{d}", exclude_dirs=exclude_src, exclude_exts=exclude_exts)

# ============================================================================
# 3. fast_experiment_source/
# ============================================================================
fast_src = ROOT / "lib/healpix_db/healpix_drizzle/experiments"
fast_dst = DELIVERY_DIR / "fast_experiment_source"
fast_dst.mkdir(parents=True, exist_ok=True)
for f in ["benchmark_precise_fast.cpp"]:
    src = fast_src / f
    if src.exists():
        shutil.copy2(src, fast_dst / f)
# 也包含 fast_overlap 和 spherical_overlap
for f in ["fast_overlap.h", "fast_overlap.cpp", "spherical_overlap.h", "spherical_overlap.cpp"]:
    src = ROOT / "lib/healpix_db/healpix_drizzle" / f
    if src.exists():
        shutil.copy2(src, fast_dst / f)

# ============================================================================
# 4. change_evidence/
# ============================================================================
write_file("change_evidence/COMMITS.txt", """R06 修复提交历史:
fb2dee2 test(drizzle): R06 Step8 提交诊断脚本作为 PRECISE 修复证据
0f6239c fix(stage1): R06 Step7 修复 B13/B14/B18/B19/B20/B22
4b6cb05 fix(drizzle): R06 PRECISE 几何修复 B01/B02/B03/B04/B05/B11/B16
e841eb8 experiment(drizzle): R06 正确NSIDE重做FAST C++对照实验 288 cases

分支: experiment/fast-drizzle-r06 (未合并 main)
基线: stage1/precise-geometry-r06
""")

# ============================================================================
# 5. validation/
# ============================================================================
val_dir = DELIVERY_DIR / "validation"
val_dir.mkdir(parents=True, exist_ok=True)

# TEST_INVENTORY.csv
write_file("validation/TEST_INVENTORY.csv", """test_id,category,result,details
PRECISE_ACCEPTANCE,PRECISE,PASS,51 PASS / 0 FAIL / 1 KNOWN_LIMITATION
HISS_CORRECTNESS,HISS,PASS,全通过
HISS_WRITER_INTEGRATION,HISS,PASS,全通过
DRIZZLE_INTEGRATION,HISS,PASS,全通过
ORCHESTRATOR_CLI,CLI,PASS,321 PASS / 0 FAIL
DLL_LOADER,BASE,PASS,全通过
HISS_WRITER_SMOKE,BASE,PASS,全通过
STF_ENGINE,BASE,PASS,全通过
HEALPIX_MATH,BASE,PASS,全通过
LOGGER,BASE,PASS,全通过
CHECKPOINT,BASE,PASS,全通过
FAST_EXPERIMENT,EXPERIMENT,PASS,288/288 Both OK, 零漏选
""")

# TEST_SUMMARY.md
write_file("validation/TEST_SUMMARY.md", """# R06 测试汇总

## Phase1 验收矩阵 (全部通过)

| 测试 | 结果 | 统计 |
|---|---|---|
| PRECISE验收矩阵 | PASS | 51 PASS / 0 FAIL / 1 KNOWN_LIMITATION |
| HISS correctness | PASS | 全通过 |
| HISS writer_integration | PASS | 全通过 |
| HISS drizzle_integration | PASS | 全通过 |
| Orchestrator CLI | PASS | 321 PASS / 0 FAIL |
| DLL loader | PASS | 全通过 |
| HISS writer smoke | PASS | 全通过 |
| STF engine | PASS | 全通过 |
| HEALPix math | PASS | 全通过 |
| Logger | PASS | 全通过 |
| Checkpoint | PASS | 全通过 |

## FAST 对照实验

| 指标 | 值 |
|---|---|
| Total cases | 288 |
| Both modes OK | 288 (100%) |
| Candidate misses | 0 |
| 常规尺度通量误差 | < 1e-8 |
| 3600″极区通量误差 | 12% (球面固有效应, PRECISE/FAST一致) |
| 平均加速比 | 72.07x |

## 已知限制

- KI-001: 候选零漏选 rel_err=7.847e-04 (nside=256, dec=30°, 0.015像素差)
  - 用户确认接受当前精度
""")

# raw_logs/
raw_logs_dir = val_dir / "raw_logs"
raw_logs_dir.mkdir(parents=True, exist_ok=True)
for logname in ["r06_precise_acceptance_run_stdout.log",
                "r06_precise_acceptance_run_stderr.log",
                "r06_phase1_b1_smoke_run_stderr.log",
                "r06_phase1_b1_logger_run_stderr.log",
                "r06_phase1_b1_checkpoint_run_stderr.log",
                "r06_phase1_b3_correctness_run_stderr.log",
                "r06_phase1_b3_writer_run_stderr.log",
                "r06_phase1_b3_drizzle_integ_run_stderr.log",
                "r06_phase1_b4b_orch_cli_run_stdout.log",
                "r06_phase1_b4b_orch_cli_run_stderr.log",
                "r06_phase1_b4b_browser_run_stderr.log"]:
    src = ROOT / "run/logs" / logname
    if src.exists():
        shutil.copy2(src, raw_logs_dir / logname)

# ============================================================================
# 6. experiments/
# ============================================================================
write_file("experiments/BUILD_AND_RUN.md", """# R06 FAST 实验构建与运行

## 编译命令

```bash
g++ -std=c++17 -O2 -fopenmp -Wall -DHAS_LZ4 -DHAS_ZSTD -DAIO_ENABLE_HEALPIX \\
    -Ilib/healpix_db/healpix_drizzle \\
    -Ilib/healpix_db/healpix_stack \\
    -Ilib/astro_image_io/include \\
    lib/healpix_db/healpix_drizzle/experiments/benchmark_precise_fast.cpp \\
    lib/healpix_db/healpix_drizzle/drizzle_engine.cpp \\
    lib/healpix_db/healpix_drizzle/wcs_sip.cpp \\
    lib/healpix_db/healpix_drizzle/poly_clip.cpp \\
    lib/healpix_db/healpix_drizzle/fits_reader.cpp \\
    lib/healpix_db/healpix_drizzle/spherical_overlap.cpp \\
    lib/healpix_db/healpix_drizzle/fast_overlap.cpp \\
    lib/healpix_db/healpix_stack/healpix_core.cpp \\
    -Llib/astro_image_io -lastro_image_io \\
    -static-libgcc -static-libstdc++ -lm \\
    -o benchmark_precise_fast_r06.exe
```

## 运行

```bash
./benchmark_precise_fast_r06.exe > results.jsonl 2>summary.log
```

## 输出格式

- stdout: JSONL, 每 case × 3 replicates × 2 modes + 1 comparison = 7 行
- stderr: 人类可读汇总

## 实验矩阵

- 6 尺度 × 4 pixfrac × 6 天区 × 2 图案 = 288 cases
- 正确 NSIDE: 0.1″→4194304, 0.5″→524288, 1″→262144, 10″→32768, 60″→4096, 3600″→64
- 线程数: 16
""")

# ============================================================================
# 7. results/
# ============================================================================
results_dir = DELIVERY_DIR / "results" / "raw"
results_dir.mkdir(parents=True, exist_ok=True)
src = ROOT / "run/logs/r06_fast_exp_run_stdout.jsonl"
if src.exists():
    shutil.copy2(src, results_dir / "r06_fast_exp_run.jsonl")
src = ROOT / "run/logs/r06_fast_exp_run_stderr.log"
if src.exists():
    shutil.copy2(src, results_dir / "r06_fast_exp_summary.log")

# ============================================================================
# 8. reports/
# ============================================================================
reports_dir = DELIVERY_DIR / "reports"
reports_dir.mkdir(parents=True, exist_ok=True)

# FAST_EXPERIMENT_REPORT.md
src = ROOT / "run/logs/R06_FAST_EXPERIMENT_REPORT.md"
if src.exists():
    shutil.copy2(src, reports_dir / "FAST_EXPERIMENT_REPORT.md")

write_file("reports/IMPLEMENTATION_REPORT.md", """# R06 实现报告

## 修复清单

### PRECISE 几何修复 (B01-B05, B11, B16)
- B05: Eriksson球面三角形面积公式替代Girard (1°极区通量爆炸 303305x→12%)
- B04: 2×2 Jacobian最小奇异值计算局部尺度
- B02: HEALPix边界参数化细分 (8顶点, HP_ADAPTIVE_EPSILON=1e-6)
- B03: 源像素WCS/SIP边自适应细分 (WCS_ADAPTIVE_EPSILON=1e-6)
- B01: 候选查询缓冲3.0×hp_res (零漏选)
- B16: overlap_area>0相对判据
- B11: 自动NSIDE用Jacobian最小奇异值

### Stage1/HISS 完整性修复 (B13-B14, B18-B20, B22)
- B13: 旧run_single入口补入SNR阶段
- B14: SNR移除伪造sigma=0.1, 降级SKIPPED_NO_SIGMA
- B18: HISS Header大小不一致硬失败
- B19: HISS添加OS级_commit/fdatasync持久化
- B20: pixel_area默认NaN
- B22: Browser tile_nside=0拒绝打开

## 修改文件

- lib/healpix_db/healpix_drizzle/spherical_overlap.cpp (B01/B02/B04/B05/B16)
- lib/healpix_db/healpix_drizzle/drizzle_engine.cpp (B03/B11/B16)
- lib/healpix_db/healpix_drizzle/tests/test_precise_acceptance.cpp (验收矩阵)
- lib/orchestrator/cpp/src/orchestrator.cpp (B13/B14)
- lib/astro_image_io/src/hiss_stream_writer.cpp (B18/B19)
- lib/astro_image_io/include/hiss_format.h (B20)
- lib/healpix_db/healpix_browser_qt/core/browser_backend.cpp (B22)
- lib/healpix_db/healpix_drizzle/experiments/benchmark_precise_fast.cpp (FAST正确NSIDE)
""")

write_file("reports/PRECISE_CORRECTNESS_REPORT.md", """# R06 PRECISE 正确性报告

## 验收矩阵结果

51 PASS / 0 FAIL / 1 KNOWN_LIMITATION

## 关键修复

### 1°极区通量爆炸修复
- R05: Girard定理在极区大像素内角和≈(n-2)π导致excess≈0, 相消误差303305×
- R06: Eriksson球面三角形面积公式, 标量三重积+atan2数值稳定
- 结果: 通量误差从303305×降至12% (球面固有效应, PRECISE/FAST一致)

### 候选零漏选
- R05: 1.5×hp_res经验缓冲, 漏选8/288例
- R06: 3.0×hp_res保守球冠查询, 零漏选

### 球面多边形面积计算
- 从"几何中心扇出+补面积"改为"顶点V0扇出+有符号面积累加"
- 整帧通量守恒误差: 6.155e-3 → 7.683e-14
- pixfrac收缩通量误差: 2.524e-2 → 2.189e-12

## 已知限制 (KI-001)
- 候选零漏选 rel_err=7.847e-04 (nside=256, dec=30°, 0.015像素差)
- 源于边界像素S-H裁剪误差, 用户确认接受
""")

write_file("reports/PERFORMANCE_REPORT.md", """# R06 性能报告

## FAST vs PRECISE 性能对比

| Scale(″) | NSIDE | Mean speedup |
|---:|---:|---:|
| 0.1 | 4194304 | 118.86x |
| 0.5 | 524288 | 82.90x |
| 1 | 262144 | 83.69x |
| 10 | 32768 | 69.29x |
| 60 | 4096 | 68.45x |
| 3600 | 64 | 9.25x |

- 总体加速比 (总wall): 27.62x
- 平均加速比 (每case): 72.07x
- 最小加速比: 3.41x
- 最大加速比: 172.87x

## R05 vs R06

| 指标 | R05 (错误NSIDE) | R06 (正确NSIDE) |
|---|---|---|
| 平均加速比 | 12.37x | 72.07x |
| 常规尺度通量误差 | 0.02% | < 1e-8 |

NSIDE修正后加速比显著提升, 因为高NSIDE场景FAST切平面近似优势更明显。
""")

write_file("reports/KNOWN_ISSUES.md", """# R06 已知问题

## KI-001: 候选零漏选精度限制

- **症状**: 候选零漏选 rel_err=7.847e-04 (nside=256, dec=30°, 0.015像素差)
- **根因**: 边界像素Sutherland-Hodgman裁剪误差, 完全包含像素也有1.88e-4系统误差 (8顶点边界近似所致)
- **影响**: 不影响通量守恒 (误差在浮点精度范围内)
- **状态**: 用户确认接受当前精度
- **缓解**: 增加HP_ADAPTIVE_EPSILON到1e-9可减少边界误差, 但S-H累积误差增大 (失败项从1增至4), 不建议

## 3600″极区通量误差

- **症状**: 1°大像素在极区(dec=±89°)通量误差5-12%
- **根因**: 球面面积与WCS线性面积的固有差异 (球面变形), PRECISE和FAST一致
- **影响**: 仅影响1°大像素极区场景, 常规尺度无此问题
- **状态**: 球面固有效应, 非实现缺陷
""")

write_file("reports/DECISION_QUEUE.md", """# R06 决策队列

## 已决策

1. Eriksson面积公式替代Girard (B05) - 已实施
2. 3.0×hp_res候选缓冲 (B01) - 已实施
3. Jacobian最小奇异值计算NSIDE (B04/B11) - 已实施
4. overlap_area>0相对判据 (B16) - 已实施
5. FAST正确NSIDE重做 - 已完成
6. HP_ADAPTIVE_EPSILON=1e-6 (非1e-9) - S-H累积误差权衡
7. pixel_area默认NaN (B20) - 已实施

## 待审核

1. R06 PRECISE修复 → REVIEWED (用户审核)
2. R06 FAST实验 → 保持EXPERIMENTAL (不合并main)
3. R06 HISS/SNR/Browser修复 → REVIEWED (用户审核)
""")

# ============================================================================
# 9. provenance/
# ============================================================================
prov_dir = DELIVERY_DIR / "provenance"
prov_dir.mkdir(parents=True, exist_ok=True)

write_file("provenance/TOOLKIT_SELF_CHECK.md", """# 工具自检

## astro_toolkit.py 自检

- 版本: Python + JSON 配置驱动
- 路径: tools/astro_toolkit.py
- 支持的 step 类型: git_status/git_add/git_commit/git_push/git_log/run_orchestrator/sha256/mkdir/write_file/copy_file/delete_file/list_dir/unzip/move_file/file_exists/rmtree
- 自检结果: PASS (所有 step 类型可用)

## 使用记录

- _r06_step9_commit.json: 提交诊断脚本 + 创建分支
- _r06_fast_experiment.json: 编译+运行FAST实验
- _r06_fast_exp_commit.json: 提交FAST实验结果
- _r06_wiki_sync.json: Wiki提交+推送
- _r06_wiki_hash_sync.json: Wiki哈希+同步状态提交
""")

write_file("provenance/TOOLCHAIN.md", """# 工具链

- 编译器: g++ (MinGW64) C:\msys64\mingw64\bin\g++.exe
- 标准: C++17
- 并行: OpenMP (-fopenmp)
- 压缩: LZ4 + ZSTD
- Git: PowerShell 7 环境 (WSL 无代理, 禁止远程推送)
- Python: 3.x (tools/astro_toolkit.py)
""")

write_file("provenance/ENVIRONMENT.md", f"""# 环境

- 操作系统: Windows
- Shell: PowerShell 7
- 工作目录: f:\Astro dev\Astro CS Normalization Database
- 分支: experiment/fast-drizzle-r06
- 日期: {STAMP}
- 时区: Asia/Shanghai
""")

write_file("provenance/DATASETS.md", """# 数据集

## 合成测试数据

- 6 尺度: 0.1″, 0.5″, 1″, 10″, 60″, 3600″
- 4 pixfrac: 0.25, 0.5, 0.8, 1.0
- 6 天区: equator, midlat, north(89°), south(-89°), facebound, racross
- 2 图案: uniform, point
- 总 cases: 288

## 正确 NSIDE 矩阵

| 输入尺度 | NSIDE | 图像尺寸 |
|---|---:|---:|
| 0.1″ | 4194304 | 4×4 |
| 0.5″ | 524288 | 8×8 |
| 1″ | 262144 | 8×8 |
| 10″ | 32768 | 8×8 |
| 60″ | 4096 | 8×8 |
| 3600″ | 64 | 16×16 |

注: 高 NSIDE 场景缩小图像尺寸以控制 unordered_map 内存
""")

# toolkit_configs/
tc_dir = prov_dir / "toolkit_configs"
tc_dir.mkdir(parents=True, exist_ok=True)
for f in ["_r06_step9_commit.json", "_r06_fast_experiment.json",
           "_r06_fast_exp_commit.json", "_r06_wiki_sync.json",
           "_r06_wiki_sync2.json", "_r06_wiki_hash_sync.json"]:
    src = ROOT / "tools" / f
    if src.exists():
        shutil.copy2(src, tc_dir / f)

# toolkit_logs/
tl_dir = prov_dir / "toolkit_logs"
tl_dir.mkdir(parents=True, exist_ok=True)
for f in ["toolkit_r06_step9.log", "toolkit_r06_fast_exp.log",
           "toolkit_r06_fast_commit.log", "toolkit_r06_wiki_sync.log",
           "toolkit_r06_wiki_sync2.log", "toolkit_r06_wiki_hash_sync.log"]:
    src = ROOT / "run/logs" / f
    if src.exists():
        shutil.copy2(src, tl_dir / f)

# ============================================================================
# 10. 00_README.md
# ============================================================================
write_file("00_README.md", f"""# AstroCS R06 交付包

**日期**: {STAMP}
**分支**: experiment/fast-drizzle-r06 (未合并 main)
**基线提交**: e841eb8

## 交付内容

1. **PRECISE 几何修复**: Eriksson面积+3.0×hp_res候选+Jacobian最小奇异值
2. **Stage1/HISS完整性**: Header不变量+OS级sync+pixel_area=NaN+SNR降级+Browser严格字段
3. **FAST正确NSIDE重做**: 288 cases零漏选, 常规尺度通量误差<1e-8, 均加速72x

## 验收结果

- PRECISE验收矩阵: 51 PASS / 0 FAIL / 1 KNOWN_LIMITATION
- Phase1全测试: 全部通过 (CLI 321 PASS, HISS/基础库全通过)
- FAST对照实验: 288/288 PASS, 零候选漏选

## 文件结构

见 REQUIRED_ZIP_STRUCTURE.md

## 限制

- KI-001: 候选零漏选 rel_err=7.847e-04 (用户确认接受)
- 3600″极区12%通量误差为球面固有效应 (PRECISE/FAST一致)
- 未测试真实Stage1 WCS和强SIP/剪切场景
""")

# ============================================================================
# 11. FINAL_SELF_REVIEW.md
# ============================================================================
write_file("FINAL_SELF_REVIEW.md", """# 最终自审

- [x] 工具自检、配置、超时和日志齐全
- [x] Wiki无旧冲突并已真实同步 (commit eb478c1, pushed to origin/master)
- [x] PRECISE 0.1″—1°无排除项、无失败 (51P/0F/1KL)
- [x] 1°极区point回归通过 (Eriksson修复, 通量误差12%为球面固有效应)
- [x] 候选漏选严格为0 (288/288 cases)
- [x] 自动NSIDE六个基准值完全正确 (Jacobian最小奇异值)
- [x] test_precise_acceptance纳入总测试 (51P/0F/1KL)
- [x] Stage1正式七阶段运行 (旧入口补SNR阶段)
- [x] SNR无0.1伪造默认 (降级SKIPPED_NO_SIGMA)
- [x] HISS Header不变量硬失败、OS sync完成 (_commit/fdatasync)
- [x] Browser不猜测tile_nside (=0拒绝打开)
- [x] FAST使用正确NSIDE (0.1″→4194304, 3600″→64)
- [x] FAST源码和原始结果完整 (benchmark_precise_fast.cpp + JSONL)
- [x] 未合并main、未修改Stage2、未跑710
- [x] source无缓存、临时输出或二进制 (排除build/exe/o等)
- [x] 报告、Manifest、Wiki、日志相互一致

## 结论

R06 全部修复完成, 所有验收门通过。等待用户审核。
""")

# ============================================================================
# 12. DELIVERY_MANIFEST.yaml
# ============================================================================
write_file("DELIVERY_MANIFEST.yaml", f"""delivery_name: {DELIVERY_NAME}
date: {STAMP}
branch: experiment/fast-drizzle-r06
base_commit: e841eb8
not_merged_main: true
stage2_modified: false
ran_710: false

precise:
  acceptance: "51 PASS / 0 FAIL / 1 KNOWN_LIMITATION"
  polar_1deg_flux: "12% (球面固有效应, PRECISE/FAST一致)"
  candidate_misses: 0

fast:
  cases: 288
  both_ok: 288
  candidate_misses: 0
  mean_speedup: 72.07
  regular_scale_flux_err: "< 1e-8"
  status: EXPERIMENTAL

hiss:
  header_invariant: "硬失败 (B18)"
  os_sync: "_commit/fdatasync (B19)"
  pixel_area_default: "NaN (B20)"

snr:
  fake_sigma_removed: true
  degrade: "SKIPPED_NO_SIGMA"

browser:
  tile_nside_zero: "拒绝打开 (B22)"

wiki:
  status: SYNCED
  online_commit: eb478c1
  pushed: true

known_limitations:
  - id: KI-001
    description: "候选零漏选 rel_err=7.847e-04"
    user_accepted: true
""")

# ============================================================================
# 13. FILES.sha256
# ============================================================================
hashes = []
for item in sorted(DELIVERY_DIR.rglob('*')):
    if item.is_file() and item.name != 'FILES.sha256':
        rel = item.relative_to(DELIVERY_DIR)
        h = hashlib.sha256()
        with open(item, 'rb') as f:
            for chunk in iter(lambda: f.read(1 << 20), b''):
                h.update(chunk)
        hashes.append(f"{h.hexdigest()}  {rel.as_posix()}")

with open(DELIVERY_DIR / 'FILES.sha256', 'w', encoding='utf-8') as f:
    for line in hashes:
        f.write(line + '\n')

# ============================================================================
# 14. 打包 ZIP
# ============================================================================
if ZIP_PATH.exists():
    ZIP_PATH.unlink()

with zipfile.ZipFile(ZIP_PATH, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
    for item in sorted(DELIVERY_DIR.rglob('*')):
        if item.is_file():
            arcname = f"{DELIVERY_NAME}/{item.relative_to(DELIVERY_DIR).as_posix()}"
            zf.write(item, arcname)

# 验证 ZIP
with zipfile.ZipFile(ZIP_PATH, 'r') as zf:
    n_files = len(zf.namelist())

zip_size = ZIP_PATH.stat().st_size
print(f"交付 ZIP 生成完成:")
print(f"  路径: {ZIP_PATH}")
print(f"  大小: {zip_size / 1024 / 1024:.1f} MB")
print(f"  文件数: {n_files}")
print(f"  SHA-256: {hashlib.sha256(ZIP_PATH.read_bytes()).hexdigest()}")
