# 代码基线（源码快照）

本目录只放**指针与清单**；34 MB 的源码压缩包放在 `run/` 下（该目录按仓库规范 gitignore，不入库）：

    run/baselines/astrocs-src-1f7c9e81.tar.gz

- 提交：`1f7c9e81`（本审核包定稿时的 `main`）
- 生成方式：`bash run/release-rescue/rqs-fix/pack_baseline.sh`（等价于 `git archive --format=tar.gz -o <out> HEAD`）
- 校验：见同目录 `MANIFEST.json`（含 sha256、字节数、文件数）与 `FILE_LIST.txt`（6914 个跟踪文件）
- 复现：`git checkout 1f7c9e81`（或解压该 tar.gz）
- 说明：仅含 **git 跟踪文件**，不含只读数据集（`testdata/`、`GaiaDR3SP/` 等）与 `run/` 工作产物。
