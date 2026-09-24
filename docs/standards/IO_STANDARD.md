# Astro Celestial Sphere Database（ACSD） I/O Standard

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

## science product 写盘

temp write → validate → atomic promote（同目录临时文件 + rename），
失败清理 temp。

## 文档必须明确

- partial file policy（中途失败的文件如何处理）；
- fsync/flush 要求（关键产品 flush + good 检查，如 aio_upm_write_sparse）；
- checksum/provenance（模型 hash、HiPS DATASUM、dense cache checksum）。

## 已知待改进（FINDING 挂账）

- aio_upm_write_sparse 当前为直接 trunc 写 + flush，未做 temp+rename：
  S6 修复或 backlog + rationale（见 findings.csv F-V19R2-IO-001）。

## 只读约束

- testdata/ 保持只读；运行产物统一 run/。
