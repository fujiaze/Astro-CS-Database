# V19R3 Final Status

## 交付范围

在现有 AstroCS main 执行 V19R3（AUDIT_DECISION 状态机 R3-S0..S12）：
1. R3-S0/S1：UPM control-variance/control-ivar 科学权重（SCI-UPM-WEIGHT-001，
   k_corr=1.4 由 Drizzle MC 校准 1.3883）+ UPMW-001..007 硬门
2. R3-S2：integration 零权重合同、policy/reducer 分离、ivar 缺失显式错误、
   CPU/ACR ivar 等价（ivar 模式 ACR 生产禁用）
3. R3-S3：Drizzle bounded target-ipix geometry cache（hit≈91.7%）+ 操作计数
4. R3-S4：权威文档深度（ERROR_MODEL 退出码修正、UPM/Sampler/Integration/
   Drizzle geometry 完整 checklist）
5. R3-S5/S6：final inventory 791/791 fresh 审计（carry=0、unreviewed=0）
6. R3-S7：clang --analyze 100% shipping units（163 direct + 144 header via TU；
   4 CUDA 工具例外）P0=0 P1=0（18 项 P3 已 triage）
7. R3-S8：final-HEAD WSL ASan+UBSan 矩阵 9/9 PASS（捕获并修复 akima P1）
8. R3-S9：traceability 63 contracts（broken=0、authority=0、50/50+50/50）
   + docs machine consistency 8/8（全集合）
9. R3-S10：comment hygiene 0 违规（扩展规则 + F11 语义修正）
10. R3-S11/S12：science regression PASS、Round0-6、clean tree

## 最终 Gate

PRE_RELEASE_ENGINEERING_FOUNDATION=PASS
FINAL_REAL_DATA_VALIDATION=PENDING（BASS/2x2/3x3/真实数据 → V20）

## 已知挂账（如实）

- WSL dll_loader Win32 工具例外（HMODULE/LoadLibrary；替代 MinGW build +
  clang analyze + 人工 review）
- plate_solve test_synthetic n_inliers 38<40（变换精确 RMS=0，阈值环境敏感；
  无 sanitizer 发现）
- PCAL Makefile 复制自身 P3（F-V19R2-PCAL-001）
- PSF 无独立单测（Phase1 真实链覆盖；V20 补）
