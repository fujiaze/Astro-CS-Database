# 审核包白名单与证据规范

## 1. 包名

`AstroCS_V6_AUDIT_<UTC>_<12-char-main-sha>.zip`

审核包只能由 `scripts/package_audit.py` 生成。禁止手工把仓库根目录整体压缩。

## 2. 必含

```text
00_README.md
SUMMARY.json
SOURCE_IDENTITY.json
TASK_LEDGER.csv
COMMITS.csv
FINDINGS.csv
TEST_SUMMARY.csv
RESOURCE_SUMMARY.csv
TRACEABILITY_MATRIX.csv
LARGE_ARTIFACT_MANIFEST.csv
PREVIEW_MANIFEST.csv
docs/review/*
docs/contracts/changed/*
source/changed_and_required/*
evidence/gates/*
evidence/failures/minimal/*
previews/*
MANIFEST.json
SHA256SUMS
```

源码范围是“本轮变更 + 审核这些变更所需公共 header/直接实现/测试/构建文件”，不是整个历史仓库。最终系统重构轮允许包含完整自有源码快照，但必须是单一 commit、排除第三方/数据/build，且在 SUMMARY 中明确 `snapshot_scope=full_self_owned_source`。

## 3. 禁止内容

- `.git/`、pack objects、branch/tag 历史；
- 旧审核包/控制包/归档包；
- `testdata` 原始 FITS、完整 HiPS、masters；
- `build/`、CMake cache、obj/pdb/ilk、sanitizer cache；
- core dump、crash dump、pagefile/swap；
- third_party 可重新获取源码/二进制（只给 lock/license/SBOM）；
- 凭据、remote token、SSH key、用户名密码、带 token URL；
- 未列入 manifest 的单文件 >5 MiB；
- 总包默认 >25 MiB。若完整自有源码确需更大，必须在包前失败并生成 size report，不由 Agent自行放宽。

## 4. 大产物引用

`LARGE_ARTIFACT_MANIFEST.csv` 字段：artifact_id、type、platform、absolute_path_redacted、size_bytes、sha256、source_commit、input_manifest_hash、producer_command_id、created_utc、retention、reason_not_in_package。

路径应脱敏为节点别名+相对 run path；哈希和生成命令必须足以验证。不得只写“在 Fatduck 上”。

## 5. SUMMARY.json 单一状态

最低字段：

```json
{
  "schema": "astrocs.audit-summary/v1",
  "version": "0.10.0-alpha.1",
  "verdict": "NOT_READY|READY_FOR_OWNER_REVIEW|ALPHA_RELEASE_APPROVED",
  "source_commit": "40hex",
  "origin_main_commit": "40hex",
  "control_sha256": "64hex",
  "ledger_sha256": "64hex",
  "task_counts": {},
  "gate_status": {},
  "science_changed": false,
  "blockers": [],
  "not_verified": [],
  "linux": {},
  "windows": {},
  "artifacts": [],
  "owner_review": "PENDING|APPROVED|REJECTED"
}
```

task_counts 必须由 ledger 计算；任何 BLOCKED/FAIL/NOT_STARTED（除 REL-004 在 owner review 前）都不能 READY。source/origin/evidence/artifact commit 不一致失败。

## 6. FINDINGS.csv

字段：finding_id、severity、category、contract_id、file、symbol、evidence、impact、required_task、status、resolution_commit。P0/P1 必须为 CLOSED 才 READY；DEFERRED 不能用来隐藏首发硬门。

## 7. 测试与资源摘要

- TEST_SUMMARY 每行对应现场 report hash，不复制历史结论；
- 记录 NOT_RUN/FAIL，不允许只收 PASS；
- RESOURCE_SUMMARY 每个 CPU-heavy module 至少一行；workers、CPU mean/p50、memory、IO、wait、bottleneck、gate status 完整；
- 32R 只引用完整报告，审核包含摘要和小预览。

## 8. 打包二次验证

1. staging 白名单复制；
2. 扫敏感内容、禁止扩展名、symlink、绝对路径、文件大小；
3. 生成 MANIFEST/SHA；
4. ZIP；
5. 解压到新临时目录；
6. `sha256sum -c`；
7. `validate_audit.py`；
8. 报 zip path/size/hash。

任何一步失败不交付半成品。
