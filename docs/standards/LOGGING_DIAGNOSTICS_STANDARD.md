# AstroCS Logging & Diagnostics Standard

- 日志统一写 run/logs/<module>/<YYYYMMDD>/，禁止写源码目录。
- 每 stage 记录：stage_id、run_id、frame_id、config_hash、input hash、
  output hash、wall/cpu/RSS/IO、thread count、status、error category/code、
  upstream cause。
- 禁止记录敏感信息（路径隐私、凭据）。
- 诊断工具：tools/astrocs_diagnose.py <run_dir> 输出小 bundle。
- 错误必须可定位：error code → troubleshooting 条目 → source/test。
