# EXTERNAL_CHANGES.md — BAS-001 外部修改登记

生成时间：2026-08-30（UTC）
起点快照：`git status --porcelain=v2` 共 205 行（5 个已跟踪修改 + 200 个未跟踪条目）

## 1. Git 状态结论

| 项 | 值 | 处置 |
|---|---|---|
| HEAD | `ac2d230da9e4f1096d98ad3cfa9a8605f84fb33f` | 保留，快进 push 对齐 |
| main | 同 HEAD | 保留 |
| origin/main | `587fe0e341a780da726917f40ed77f610de0c73f`（= 控制包冻结基线） | push 后对齐 |
| 本地领先 origin | 1 commit（`ac2d230` 审核包打包工具 tools/pack_audit_package.py，仅新增 116 行工具，不触碰科学/生产代码） | 保留并 push（快进，非 force） |
| submodule | 无有效 submodule（`.gitmodules` 缺失但 `AstroCS.wiki` 为普通目录） | 登记 |
| LFS | 未安装/未使用 | 登记 |

## 2. 已跟踪文件修改（保留，不覆盖）

| 文件 | 修改内容 | 来源判断 | 处置 |
|---|---|---|---|
| artifacts/prerelease_v5/ISA-001/MEASUREMENTS.csv | benchmark 数值更新（calibration +9.5→+21.4% 等 6 行） | V5 ISA 证据数据，工作区更新 | 保留不动 |
| artifacts/prerelease_v5/ISA-002/MEASUREMENTS.csv | 同上 | 同上 | 保留不动 |
| artifacts/prerelease_v5/ISA-003/MEASUREMENTS.csv | 同上 | 同上 | 保留不动 |
| artifacts/prerelease_v5/ISA-004/MEASUREMENTS.csv | 同上 | 同上 | 保留不动 |
| tests/cli/test_bench_cli.py | `COMMIT` 常量改为从 `--version` 的 `+g<hash12>` 反查二进制内嵌 commit（测试增强，不改变被测语义） | 工作区更新 | 保留不动 |

以上 5 个文件的修改与 V6 重构任务无重叠（V6 不涉及 V5 ISA 证据数据与 bench CLI 测试），登记后不在 V6 任务中触碰。

## 3. 未跟踪文件/目录（199 条目，均保留）

- 根目录旧控制包/审核包 zip 与 md：AstroCS_MAIN_*、AstroCS_REAUDIT_*、AstroCS_V5_AUDIT_REVIEW_20260830.zip、AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830.zip
- artifacts/prerelease_v5/：AUDIT_PACKAGE_587fe0e341a7.zip、audit_src/、capsules/*.zip（39 个）、lnx_pkg/
- 根目录运行产物：astrocs_run_*.json（约 150 个）、cpu_profile.json
- evidence/（本次 V6 证据目录，新建）
- 工程控制/CONTROL_V6/（本次控制包解压目录，新建）

以上均属历史证据/运行产物/控制包材料，V6 任务不 stage、不清理。

## 4. 承诺

- 不覆盖、不清理上述任何外部修改；
- V6 任务只 stage 各自任务声明的文件；
- push 仅快进，禁止 force/rebase/reset --hard。
