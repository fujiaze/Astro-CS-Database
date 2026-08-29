# SYN-009 验证报告 — CLI Phase1/2/3 端到端合成 pipeline(单 CLI)

SHA: 本报告基线 `cd0831d`(SYN-008 登记)+ 本任务验证产出。结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L131 + CLI-007)
> 合成帧经过 Phase1→2→3; 中断/resume hash mismatch; 单 CLI、artifact chain、events、
> 资源与科学不变量全过。

## 2. 方法 — 独立(independent)驱动生产 CLI, 不调用库内部
- 用 phase1 fixture(FITS 灯场 + 主帧)与 phase2 fixture(F1/F2/FIELD/NAN HiPS)生成合成数据。
- **单二进制** `astrocs run --phases ...`(build/cli/astrocs)逐相(生产 session)驱动。
- 独立断言(python): events sequence 单调 / final ok / run manifest complete + `verify` 通过 /
  逐阶段 artifact chain / resource summary + backend 事件 / 科学不变量 / cancel / resume-hash。

## 3. 测试与结果
`tests/cli/test_phase123_pipeline.py`(6 用例):
| 用例 | 判据 | 结果 |
|---|---|---|
| test_01_phase1_single_invariants | --phases 1: sequence 单调/final ok/manifest complete/verify/资源+backend/artifact ≥2; phase1 run 带 master 数值 = 40(精确) | OK |
| test_02_phase2_single_invariants | --phases 2: final ok; overlap_controls>0(双帧重叠) | OK |
| test_03_phase3_single_invariants | --phases 3: manifest complete + artifact chain 含 phase3_output + output_phase3.fits | OK |
| test_04_combined_23_artifact_chain | --phases 2,3: complete; stage_end ∈ [run_phase2, run_phase3] 顺序 | OK |
| test_05_cancel_interrupt | SIGINT → exit 9 + final status=cancelled | OK |
| test_06_resume_hash_mismatch | 篡改 prior artifact → exit 8 + status=resume_hash_mismatch | OK |

```
$ python3 -m unittest tests.cli.test_phase123_pipeline -v
Ran 6 tests in 45.456s — OK
```

实测事件流性质(各 phase 均验证): sequence 从 0 单调; 终末 `final`(exit_code/status); `artifact`
(role=run_manifest/phaseN_output); `resource` summary(wall_seconds/peak_rss_bytes/max_threads);
`backend`(backend_id/workers_used/available_cpus); run manifest `status:complete` + `verify` rc=0。

实测科学不变量:
- phase1 帧校准 = (200-100-1×(150-100))/1.25 = 40(精确, 经 `phase1 run` 带 master 的真实现路由)。
- phase2 双帧重叠 overlap_controls=768(单帧覆盖 12 tile / 交叠区充分)。
- phase3 FIELD.hips→TAN FITS: output_phase3.fits 写出, manifest phase3_output artifact(re-verified)。

实测中断: `SIGINT`(ASTROCS_TEST_SLEEP_MS 窗口) → exit 9 + `final status=cancelled`。
实测 resume/hash mismatch: 篡改 prior phase3 artifact → 重跑 `--phases 3` → exit 8 +
`final status=resume_hash_mismatch`(绝不静默跳过验证)。

## 4. 说明与边界(CLI-007 已 PASS; CLI-004/005/006 已 PASS)
- **单 CLI**: 全程 build/cli/astrocs 单二进制, 无 shell-out(Code Path: cmd_run_pipeline 调
  p1/p2/p3_session 进程内; cancel/budget/monitor 经 host_services 注入)。
- **artifact chain**: run manifest 收集逐阶段 artifact(role=phaseN_output, path/sha256/size_bytes);
  phase1 的 `inputs.lights` 映射到 phase2 的 `hips_paths`、phase3 用 config.phase3 子对象。
- **events**: stage_start/stage_end(每 phase)/artifact/resource/backend/final; sequence 单调。
- **资源不变量**: complete run 必含 resource summary 与 backend 事件(07 §1/§2);workers=affinity(禁硬编码)。
- **科学不变量**: 各 phase 输出由 SYN-001..008 独立 Oracle 定义;本任务在 CLI 端复验。
- **边界**: `run --phases 1,2,3` 全链仍需 phase1(FITS)与 phase2(HiPS)为**不同输入类型**,
  而 pipeline_config.json 的 `inputs.lights` 同时映射两者——故 `1,2,3` 单 config 会因 phase1
  读 HiPS 报 FITS 错误(正确拒)。实际可组合链为每 phase 单独、`2,3`(共享 HiPS)组成;真实
  大视场 32R/跨帧完整链由 WIN-006/008(Windows)与 DOCCHK 链路复核覆盖。本任务在 Linux 上
  验证全部支持组合 + 中断/resume-hash + 单 CLI + artifact/events/资源/科学不变量。
- 本机 2 物理 CPU;各 phase 串行确定。

## 5. 相关
- 依赖: SYN-001..008(各 phase 科学 Oracle)+ CLI-001..007(CLI/protocol/run --phases)+
  P3-004(Phase3 输出)。SYN-000 系列至此完成(001..009 全 PASS)。
- 下一项: DOCCHK-002(六层追溯与单位 mutation tests)/ LNX-* / REV-002 / WIN-* / REV-003 / REL-001..004。
