# Round 6 — Clean-Tree Final Verification（V17）

日期：2026-08-14 ｜ 规则：全新（非增量）构建 + 全量 gate + 真实 16 帧
E2E + 外部 browser + oracle + 受控 truth + no_legacy。

## 1. Clean build（全新 build 目录）

```text
cmake -S lib/phase2 -B run/temp/p2_v17_clean_build -G Ninja
cmake --build run/temp/p2_v17_clean_build -j 8
orchestrator: lib/orchestrator/cpp make（V17 legacy-removal 修复后 rc=0）
```

V17 追加：CMake POST_BUILD 复制 acr_cuda_bridge.dll 到 exe 目录（bridge
loader 搜索路径 1），保证 clean build 与增量环境一致；初次 clean build 的
CudaEquivalent 因缺 DLL 失败已由此修复（其余 CUDA 测试本身 GTEST_SKIP）。
编译 24.4s，产物：phase2_synthetic_gate.exe / astrocs-stage2.exe /
rejection_cli.exe（+ acr_cuda_bridge.dll）。

## 2. 全量 synthetic gate

```text
run/temp/p2_v17_clean_build/phase2_synthetic_gate.exe
→ 74/74 PASS（45.8s；含 V17NonFinite*/V17Statuses*/V17InvalidMethod*/
   V17LargeScale* 5 项；CudaEquivalent 含 GPU 等价）
evidence/full_gate.log 归档。
```

## 3. 真实 16 帧 Phase1→Phase2 E2E

```text
Phase1: orchestrator × 16（before_full_cold / after_full_warm，全 rc=0）
Phase2: real16 四组（truth/clean/trail/trail_none）V17 二进制重跑全部 rc=0
```

after_full_warm：16 帧全部 rc=0（wall median 142.4s / p95 149.9s）；
两组各 16 帧 wall 与阶段 profile 见
reports/phase1_performance.md 与 evidence/performance_phase1_*.

## 4. 受控 clean rejection truth

```text
20 帧零离群合成 → truth(none) + auto(wbpp_2_9_1) stage2 rc=0；
true FPR 1.88%（Siril 100% 一致）；注入 recall 1.0/1.0/1.0；
large_scale：satellite grown=3079、cosmic grown=0。
```

## 5. 外部 HiPS / browser

```text
test_hips_browser_backend.exe real16/mosaic_trail.hips → RESULT: PASS
browser_cli --benchmark：cold 75.2ms / pan p50 34.7ms / zoom p50 44.9ms
```

## 6. Oracle / 一致性 / legacy

```text
oracle_matrix.json：无 REFERENCE_NOT_RUN；IRAF NOT_CLAIMED
config_consistency.py PASS（含 large_scale 4 字段）
api_doc_consistency.py PASS（10 checks）
no_legacy_production_reference.py PASS
repo_source_manifest.csv：4197 文件（path/SHA256/classification/caller）
```

## 7. 结论

```text
known P0 = 0
known P1 = 0
FINALIZATION_SELF_REVIEW = PASS
ASTROCS_FOUNDATION_FINAL_FREEZE = PASS（同步 docs/validation/SCIENCE_FREEZE.md
  与 reports/final_status.md；PIXINSIGHT_EXACT=NOT_CLAIMED 保留）
```
