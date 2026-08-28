# VER-001 执行日志 (2026-08-28, vm-bj)

## 输入
控制包 13_ALPHA_VERSION_AND_PHASE3.md §1; 03_TASK_DETAILS VER-001 行; 仓库现状扫描(无任何产品版本源)。

## 动作
1. 全仓版本字面量扫描: 无产品 semver 源(dist/AstroCS-CLI-v1 不存在; 代码仅 hips_version=1.4 等外部格式版本)。
2. 建立唯一版本源 `VERSION` = `0.9.0-alpha.1`(基线取 13 规范示例 0.9.0, 未自行改大)。
3. 生成接口 `tools/gen_version.py`: 版本串 clean=`X.Y.Z-alpha.N+g<commit12>` / dirty 追加 `.dirty`; `--version --json` 输出 version/prerelease=alpha/commit/dirty/build_id/abi_version/cli_schema_version; abi_version=cli_schema_version=0(待 ABI-001/API-002 冻结置 1)。
4. schema `schemas/version.schema.json`(draft-07 子集合同, prerelease const=alpha, 拒绝 stable/rc)。
5. 机器检查 `tools/check_version_consistency.py`: 扫描 docs/schemas/tools/launch/tests+根级文件; X.Y.Z 必须等于唯一源基础号; alpha.N 漂移必报; stable/rc/beta 必报; 显式豁免清单(外部工具/格式版本, 见 docs/VERSIONING.md §5)。
6. 测试 `tests/version/test_version_consistency.py`: 5 用例(报告过 schema、dirty 后缀、拒绝 rc、真实仓 PASS、mutation 伪造必抓+对照无误报)。

## 验证
- 首跑 checker 抓 36 处 → 校准(自身基线号比较 bug 1 处 + 外部工具豁免 + IP 127.0.0.1 误报 + 测试样本自身) → 重跑 **VERSION_CONSISTENCY_PASS**。
- unittest **5/5 OK**; gen_version --json 实测输出合法(dirty=true 反映未提交的 VER-001 文件, 属实)。
- mutation 试金石: 伪造 "1.2.3" 被抓(errs 含 1.2.3), 正确版本无误报。

## 产物
VERSION; schemas/version.schema.json; docs/VERSIONING.md; tools/gen_version.py; tools/check_version_consistency.py; tests/version/test_version_consistency.py; 本日志。

## PASS 判定
version_consistency mutation 测试成立(伪造必失败); --version --json schema 校验 PASS; 无 stable/RC; 包名/CLI/manifest 同步规则已冻结于 docs/VERSIONING.md §3(打包脚本必须调 gen_version)。VER-001 = PASS。
