# quality

机器质量检查器与 CI 驱动器的主目录：静态与合同一致性检查、构建与测试驱动、产物比对、生成器与打包审计。

## 职责边界

- 放：`check_*.py` 检查器族、`gen_*.py` 生成器族、CI 驱动器、比对与监控工具、检查器数据与夹具。
- 不放：检查项注册表（在 `eng/ci/checks.json`）、文档一致性检查器（在 `eng/tools/doccheck/`）、测试用例本体（在 `eng/tests/`）。

## 内容

- `check_*.py`（34 个）—— 检查器族：模块映射 `check_module_map.py`、ctest 注册 `check_ctest_registration.py`、结论真实性 `check_conclusion_truth.py`、已知失败基线 `known_failures_baseline.py`、日志系统、ISA 与预算单一事实源、密钥卫生、根目录清洁、稀疏打孔、复杂度与注释卫生等。
- `deep_ci_driver.py`、`ci_windows_driver.py`、`ci_coverage_runner.py` —— 深度构建、Windows 构建与覆盖率的 CI 驱动器。
- `gen_*.py` —— 生成器：模块页、运行图、提交 CSV、块流规范、源码索引。
- `build_v19r*_package.py`、`v19r3_*`、`v19r4_*` —— 审计打包、静态审计与消毒剂检查脚本。
- `compare_products.py`、`frame_qc_grid.py`、`resource_monitor.py` —— 产品比对、帧质控网格与资源监控。
- `*.json` 数据面 —— 权威面、预算来源、结论词表与快照、覆盖声明、ISA 站点登记。
- `contracts/` —— 合同检查器子族（API/配置/执行/科学量纲/测试合同等 13 个）及其夹具与量纲登记表。
- `fixtures/`、`tsan/` —— 检查器夹具与 ThreadSanitizer 抑制清单。

## 上游

- 注册于 `eng/ci/checks.json`：大量检查步骤以本目录脚本为命令入口，含 `CHK-BUILD-LINUX`、`CHK-BUILD-WIN`、`CHK-STATIC`、`CHK-MODULE-MANIFEST`、`CHK-UNIT`、`CHK-ORACLE`、`CHK-INVARIANT`、`CHK-ABI`、`CHK-CONTRACT-TEST`、`CHK-SCI-REF`、`CHK-TRUTHFUL-CONCLUSION`、`CHK-SECRET-HYGIENE`、`CHK-ROOT-CLEAN` 等，逐条以 `eng/tools/quality/` 路径登记。
- 检查项条目见 `docs/ci/01_CHECKS.md`，流水线 job 结构见 `docs/ci/02_PIPELINE.md`，门禁分级见 `docs/ci/03_GATES.md`，证据落位见 `docs/ci/04_ARTIFACTS.md`。
