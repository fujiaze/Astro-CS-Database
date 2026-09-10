# 03｜审核包规格

结论枚举：`READY_FOR_OWNER_REVIEW | NOT_READY | BLOCKED_EXTERNAL | PACKAGE_INVALID`。

## 白名单

1. 冻结宪章、活动 L0/L1；2. final tracked source manifest/必要源码快照；3. task/state/commit ledger；4. SCI→ALG→DATA/API/ARCH→symbol→TEST→evidence→L0 追踪；5. AST/API/ABI/导出符号、DAG/trace/call-count；6. Linux/Windows CI_RESULT、build provenance；7. Linux real-final 与 Fatduck 脱敏摘要、resource summary、固定参数轻量 PNG/JPG；8. candidate product manifest、SBOM、licenses、SHA256；9. findings 与最小失败复现。

## 禁止

`.git`、旧控制包集合、build/cache/worktrees、raw testdata/Gaia/FITS/完整 HiPS tiles、大索引、core、凭据、完整重复日志、用户绝对路径、重复源码。大件只记 logical ID、size、sha256、生成命令、保留位置；不可公开数据只留脱敏统计。

## 必检

审核包从空 staging 目录按白名单复制；拒绝 symlink/special file/path traversal；生成 manifest 与 SHA256SUMS；fresh extract 后复验；全部证据绑定 final SHA、candidate digest、配置 hash 和输入数据清单 hash。
