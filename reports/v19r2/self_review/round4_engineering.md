# Round4 — Engineering（ownership/thread/error/IO/perf）

- ownership：p2_upm_open 失败路径统一释放（PR 门禁修复 + F-V19R2-UPM-002）；
- thread：OpenMP 设置由 run context 控制（ENG-THREAD-001），计数器无裸
  race（扫描 0 命中）；
- error：rc/status 无双语义（unknown frame 修复），畸形模型稳定报错
  （8 类测试）；
- IO：aio_upm_write_sparse 原子化（F-V19R2-IO-001），HiPS 写/校验协议
  文档化；
- perf：热路径扫描（per-pixel alloc/log/fs）0 命中；Drizzle 候选计数
  METRIC 存在。

结论：PASS。
