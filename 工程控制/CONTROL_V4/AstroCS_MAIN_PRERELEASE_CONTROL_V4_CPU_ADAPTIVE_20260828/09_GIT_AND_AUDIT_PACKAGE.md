# Git 与最终审核包

## Git

- 仅 `main`；不创建开发/审计/prerelease分支。
- 每个 `required_commit=yes` 的 Task 恰好一个目的明确的 commit。
- 测试通过后立即 `git push origin main`，并验证 `HEAD==origin/main`。
- 不提交 testdata、benchmark cache、build、HiPS、profile series、审核包。
- 禁止 force-push、reset --hard、覆盖未知修改。

## 审核包

仅最终生成一次，目标10 MiB，硬上限25 MiB，单文件5 MiB。

必须包含：

- start/final SHA、task ledger、commit ledger；
- findings、build/test结果、traceability摘要；
- CPU profile与最终每kernel选择；
- review capsule索引、SCIENCE_CLAIMS和外部审核决定；
- 资源利用 stage摘要；
- 合成 Oracle 汇总和 tolerance hash；
- Windows build/test、32R摘要、seam和HiPS manifest；
- 大产物 manifest、MANIFEST、SHA256SUMS。

禁止包含：

- `.git`、历史bundle/source、build/binary；
- 原始 FITS/XISF、完整HiPS、像素CSV；
- 大于5 MiB日志；
- TSan/ASan原始长日志；
- benchmark原始全序列；
- 旧控制包/旧审核包。

长日志和大产物只记录路径、大小、SHA、producer commit、config/input hash。
