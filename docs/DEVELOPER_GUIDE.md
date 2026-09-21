# AstroCS 开发者指南

> 上游：`AGENTS.md` §3（环境与构建：唯一根 CMake + presets、Linux 开发节点、`python3 ci/run_checks.py`）、
> `ENGINEERING_SPEC.md` §1（语言/编译器/平台）/§6（Git 与提交：只 `main`，禁止分支/worktree）、
> `ASTROCS_DESIGN.md` §12（Alpha 前无版本信息）。

## 环境与构建

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

模块地图见 `docs/architecture/MODULE_MAP.md`；机器一致性检查见 `docs/ci/CI_SPEC.md`。

## 测试

```bash
ctest --test-dir build --output-on-failure
```

模块级科学矩阵：

```text
snr_estimator:      noise_model_science_test (SNR-001..015 矩阵)
healpix_drizzle:    variance_propagation_test (SNR-011/012 + DRZ)
phase2:             phase2_synthetic_gate (82 项, 含 PR-UPM-001..010)
astro_image_io:     pipeline_frame_contract_test / dataflow_fuzz
```

## 编码规范

- 仅修改 `lib/` 源码; 运行产物只写 `run/`（或 `output_dir`）
- 一个 commit = 一个明确目的；只 `main` 原子提交（`ENGINEERING_SPEC.md` §6）
- 修改后必须自测; 未验证不得宣称完成
- 禁止大规模 cosmetic refactor
- 日志统一写 `run/logs/<module>/<YYYYMMDD>/`
- 中文注释; 科学文档含公式/单位/假设/失效域/源码入口
- 规范标准：`docs/standards/`；追溯：`docs/traceability/TRACEABILITY_MATRIX.json`（机器真相）
