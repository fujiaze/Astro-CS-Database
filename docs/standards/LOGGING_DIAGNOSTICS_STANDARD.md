# Astro Celestial Sphere Database（ACSD） Logging & Diagnostics Standard

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

- 日志统一写 run/logs/<module>/<YYYYMMDD>/，源码目录保持只读。
- 每 stage 记录：stage_id、run_id、frame_id、config_hash、input hash、
  output hash、wall/cpu/RSS/IO、thread count、status、error category/code、
  upstream cause。
- 日志只记非敏感信息（路径隐私、凭据保留在日志之外）。
- 诊断工具：eng/tools/astrocs_diagnose.py <run_dir> 输出小 bundle。
- 错误必须可定位：error code → troubleshooting 条目 → source/test。
