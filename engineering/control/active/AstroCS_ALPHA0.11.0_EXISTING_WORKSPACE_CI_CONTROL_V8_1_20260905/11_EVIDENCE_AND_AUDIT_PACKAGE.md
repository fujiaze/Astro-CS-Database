# 11｜证据与审核包规范

## 每任务证据

```text
evidence/tasks/<TASK-ID>/
├── TASK_RESULT.json
├── commands.jsonl
├── changed_files.txt
├── test_summary.json
└── findings.csv
```

heavy 任务另含 `resource.csv` 与 `resource_summary.json`。所有证据记录完整 source SHA；PASS 只能由命令 exit code 和 schema 计算。

## 最终审核包必含

- `00_READ_FIRST.md`
- `SUMMARY.json`
- `SOURCE_MANIFEST.json`
- `source/source_tree.tar.gz`：当前 main 的 tracked 源码、活动文档、CI 配置；不含 `.git`
- `provenance/{identity,toolchains,commands,commit_ledger}.json*`
- `ci/{CI_RESULT,checks,coverage_summary,complexity_summary,actions.lock,toolchain.lock}`
- `evidence/task_state.json`、`evidence/findings.csv`、`evidence/traceability.csv`
- `github/{LINUX_CI_RESULT,WINDOWS_CI_RESULT,BUILD_PROVENANCE}.json`
- `fatduck/FATDUCK_FINAL_SUMMARY.json` 与允许公开的小型 JPG/指标；Fatduck 离线时明确 pending reason
- `docs/L0_OWNER_REVIEW.md`、L1 工程状态、L2 科学算法索引、L3 机器追踪
- `LARGE_ARTIFACT_MANIFEST.csv`
- `SHA256SUMS`

## 白名单与禁止项

禁止打包：`.git`、build/install/cache、原始 FITS、HiPS tiles、大体积索引、历史控制包/审核包、core dump、凭据、完整 runner 日志、重复源码副本。

单文件默认上限 10 MiB，总包默认上限 50 MiB；超过时必须是经白名单声明的小型必要证据。真实数据和大产物只在 `LARGE_ARTIFACT_MANIFEST.csv` 记录脱敏逻辑 ID、大小、SHA256、生成命令和保留期；不得记录不可公开的原始路径/文件名。

打包器必须从 allowlist 复制到新 staging 目录，拒绝 symlink/special file，生成哈希，再 fresh 解包复验。不得对仓库根目录直接递归压缩。

## 源码基线

审核源码包必须包括活动治理文件（AGENTS、memory、ENGINEERING_CONSTRAINTS、文档索引）、`.github/workflows`、`ci/`、CMake、源码、测试和模块 README；不能再次因 allowlist 遗漏而失去接续开发所需定义。
