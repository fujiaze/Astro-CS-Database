# AstroCS 开发者指南

> **DOC-001 状态注记（2026-09-16）**：本文标题中的 V19R8 编号、以及下文「环境/构建/Git 工作流」章节为**历史 Windows 工作流记录**，不是当前规范。
> 当前规范：`AGENTS.md` §3（环境与构建：唯一根 CMake + presets、Linux 开发节点、`python3 ci/run_checks.py`）、
> `ENGINEERING_SPEC.md` §1（语言/编译器/平台）/§6（Git 与提交：只 `main`，禁止分支/worktree）、`ASTROCS_DESIGN.md` §12（Alpha 前无版本信息）。

## 环境

```text
Windows + PowerShell 7, MSYS2 MinGW64 (g++ 16.1.0), CMake 4.3.2,
Python 3.12 (规范: py -3.12), GitHub CLI (Temp 安装)
```

## 构建

```powershell
.\toolchain.ps1 check     # 环境自检
.\toolchain.ps1 build     # 全量构建 (healpix_stack 冻结跳过)
.\toolchain.ps1 run <stage1.json>
```

模块独立构建: 各 `lib/<module>/cpp` 下 `make`（模块地图见
docs/architecture/MODULE_MAP.md）。

## 测试

```text
snr_estimator:      noise_model_science_test (SNR-001..015 矩阵)
healpix_drizzle:    variance_propagation_test (SNR-011/012 + DRZ)
phase2:             phase2_synthetic_gate (82 项, 含 PR-UPM-001..010)
astro_image_io:     pipeline_frame_contract_test / dataflow_fuzz
```

## 编码规范

- 仅修改 `lib/` 源码; 运行产物只写 `run/`
- 每最小任务 commit, 阶段完成 push
- 修改后必须自测; 未验证不得宣称完成
- 禁止大规模 cosmetic refactor (V19 PRERELEASE_CODE_QUALITY.md)
- 日志统一写 `run/logs/<module>/<YYYYMMDD>/`
- 中文注释; 科学文档含公式/单位/假设/失效域/源码入口
- 规范标准：`docs/standards/`；追溯：`docs/traceability/TRACEABILITY_MATRIX.json`（机器真相；旧 `docs/TRACEABILITY.csv` 追溯门已退役）

## Git 工作流

```text
只 main 开发（禁止分支/worktree/额外 clone；ENGINEERING_SPEC §6）; ACR 仅保留源码且 DORMANT
控制包协议: START_HERE → EXECUTION_ORDER → 实施 → 审核包 → SHA256
```
