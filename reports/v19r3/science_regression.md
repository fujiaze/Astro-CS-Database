# V19R3 Affected Science Regression

最终代码（HEAD 4396f9d）回归：

- phase2 synthetic_gate：89/89 PASS（41.8s；含 V19R3 新增 UPMW-001..007）
  - UPMW-001 snr/support 扰动权重不变
  - UPMW-002 control_ivar 1:4 → weight 1:4
  - UPMW-003 3 星群权重不变
  - UPMW-004 Var(median)≈πσ²/2N ratio=0.9968
  - UPMW-005 Drizzle MC k_corr=1.3883（缓存科学中性）
  - UPMW-006 缺 control ivar 显式 rc=2
  - UPMW-007 patch vs truth ratio=1.0425
- Drizzle candidate oracle：9003/0 失败（false_negative=0，8.7s）
- Drizzle freeze 42/42（FP64 闭合 1e-6、FP32/FP64 1e-5、点源通量 2.8e-10）
- WSL ASan+UBSan 9/9 PASS；sanitizer 捕获 akima P1 已修复
- 全仓 toolchain build 通过（healpix_stack 冻结跳过，PCAL Makefile 复制
  自身为已知 P3 F-V19R2-PCAL-001）
- 已知限制：BASS/16 帧批测按控制包要求未跑（V20 FINAL_REAL_DATA_VALIDATION）
