# 审核包白名单规范

## 体积

- 目标且自动硬门禁：压缩后 `<=25 MiB`。
- 单文件 `<=5 MiB`。
- 超限即 FAIL，不允许先打大包再人工裁剪。

## 必须包含

- `00_READ_FIRST.md`：候选 SHA、Gate、最终状态、未完成项。
- `SUMMARY.json`：由脚本从 CSV 生成，禁止手填计数。
- `TASK_LEDGER.csv`、`CHECKPOINT_RESULTS.csv`、`COMMITS.csv`。
- `FINDINGS.csv`、`BUILD_RESULTS.csv`、`TEST_RESULTS.csv`、`PERF_RESULTS.csv`。
- `TRACEABILITY.csv` 的摘要或完整文本（单文件不超限）。
- 当前 Checkpoint 所需的短日志、配置、运行命令、结果 JSON。
- `LARGE_ARTIFACT_MANIFEST.csv`：外部大产物的路径、大小、hash、生成 commit/config/input hash。
- `MANIFEST.json`、`SHA256SUMS`。

## 禁止包含

- `.git/`、git bundle、全历史、完整 source tar；
- build/builds/out/cache/temp；
- `.o/.obj/.a/.lib/.so/.dll/.exe/.pdb`；
- FITS/XISF/HISS、原始像素 CSV、完整 HiPS、模型缓存；
- 旧审核包、旧控制包、重复补丁；
- 绝对路径中的用户名、token、remote 凭据；
- 超过 5 MiB 的 patch/log。

## 大产物引用

只记录：`artifact_id,host,relative_path,size_bytes,sha256,producer_commit,config_sha256,input_manifest_sha256,created_utc,retention`。审核人按需点取，Agent 不得擅自打包。

## 固定打包命令

`python3 <control>/scripts/package_audit.py <evidence_dir> <output.zip>`

禁止用通配符 `zip -r` 或人工裁剪替代。脚本生成并验证 `MANIFEST.json`、`SHA256SUMS`，超限自动失败。

## 一致性

- `SUMMARY.json` 中所有计数必须由 CSV 重新计算。
- `00_READ_FIRST.md` 不得维护独立手填计数，只引用 `SUMMARY.json`。
- 所有状态只允许五种固定值。
- 任一引用路径不存在、hash 不一致、报告截断、状态矛盾即 FAIL。
