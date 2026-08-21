# AstroCS Changelog

## [V19R8] 2026-08-22 — Quality Closure B4 28/28 + B5 docs (76/95 DONE)
- B4 28/28 DONE: calibration/plate_solve/gaia/dynamic_psf/photometric/snr/drizzle/orchestrator/phase2 全锚点 + machine 9/9 + push闭环
- B5 6/8 DONE: PROJECT_STATE/CURRENT_TASK/DECISION/RISK/MASTER+TARC/TRAC 75/RELEASE 7件套同步 V19R8 S0-S6 (B4 gate+TRAC 75)
- TRAC 63→75 rows, `MASTER_TASK_REGISTER QA-V19R8-S3-11` 追加

## [V19R2] 2026-08-15 — Pre-Release Traceable Foundation

### PR#1 UPM 持久化绑定（SCI-UPM-PERSIST-001 门禁）
- 修复 save/load 后 frame_id→参数绑定漂移：frames 列表改用
  frame_id_by_index（index 序）序列化，open 重建双向映射
- p2_upm_open 畸形模型校验：重复 ID/缺列表/行列不匹配/类型损坏稳定报错
  （ERR-P2-UPM-001）；p2_upm_save 校验行数不变量
- PR-UPM-001..010 门禁测试全部通过；向后兼容方案 A（安全迁移）

### 工程冻结（全仓规范 + 逐文件审计 + 质量）
- 文档体系 L0-L5：science/11、algorithms/12、architecture/12、
  standards/13、modules/13；V19 扁平文档迁 docs/history/
- TRACEABILITY.csv 30 行；science_code_mapping.csv；broken=0
- 逐文件审计 713/713（B01-B16），0 UNREVIEWED
- comment hygiene：1240 处轮次/审计标记迁移，434 文件 0 violation
- 编译告警：phase2 补 -Wall -Wextra -Wpedantic，全仓 0 first-party warning
- 修复：unknown UPM frame 显式失败（F-V19R2-UPM-002）、aio_upm 原子写
  （F-V19R2-IO-001）、strncpy 截断（F-V19R2-COV-001）、死代码
  （F-V19R2-REJ-001）、DRZ SIP 测试阈值（F-V19R2-DRZ-001）
- 自审 Round0-6 闭环；P0=0 / P1=0

```text
PRE_RELEASE_ENGINEERING_FOUNDATION=PASS
FINAL_REAL_DATA_VALIDATION=PENDING
```

## [V19] 2026-08-15 — Pre-Release Foundation Closure

### SNR/Noise 科学重构
- 三层模型: PhotometricCalibrationQuality / PsfFitQuality / NoiseWeightModelV1
- NoiseWeightModelV1: source-masked blank-sky 稳健方差 → ivar, 空间平面场,
  gain/read-noise 交叉验证, 经验 fallback
- 旧 `(A-B)/mad` 从科学路径退休 (pedestal invariance 反例, SNR-008)
- 科学矩阵 SNR-001..015: 模块级 32/32 + Drizzle MC 8/8 + phase2 ablation

### Drizzle 深度优化
- 方差传播: sumVarNum += v_j·w_jp², variance/ivar HiPS 产品 (P1-003)
- 操作计数: operation_counts.json (candidates/overlaps/pix2radec/...)
- 科学: SNR-011 MC 4000 实现 p50=1.001, p95=[0.962,1.042]

### Phase2 权重模型
- UPM: quality × support^p × ivar (legacy snr² 仅 ablation)
- Integration: weight_mode=ivar 默认; support 只作 validity
- ACR kernel: mode 2 = support×ivar

### 代码质量 / 文档
- 诊断工具 tools/astrocs_diagnose.py + ERROR_TAXONOMY/DIAGNOSTICS/
  TROUBLESHOOTING
- docs/ 全套开发/科学/发布文档 (18 文件)

### 验收
- phase2 synthetic gate 74/74
- V19 判定: PRE_RELEASE_FOUNDATION_READY=PASS (V20 才签
  PRE_RELEASE_CANDIDATE_READY / FINAL_REAL_DATA_VALIDATION)

## [V18R3] 2026-08-14 — Gaia polar prune/cache/memory-safety/trace

- 保守极冠剪枝、精确 cache key、事务替换、checked public allocation
- 全 first-party warnings 零 (-Wall -Wextra -Wpedantic, 87 compile units)
- GAIA_V18R3_CORRECTNESS_BASELINE=ACCEPTED

## [V18R2] 2026-08-13 — Resource-driven performance closure

- Phase1 ~67.35 s/frame, Drizzle ~64 s/frame (16-frame batch 基线)
- 15 红队假设关闭

## [V17] 2026-08-14 — True Final Freeze (Phase2)

- integration status 显式、large-scale rejection、non-finite 硬门

## [V16] 2026-08-14 — Final Closure AuditFix

- wbpp_current/astrocs_adaptive profile 拆分、RejectionNormalizationPolicy
- 真实 16-exposure E2E, gate 65/65

## [V15] 2026-08-14 — Final Semantic Closure

- rejection 语义冻结、satellite gate、WBPP 2.9.1 策略对齐

## [V14] 2026-08-13 — Phase1/Phase2 Finalization

## [V13] 2026-08-12 — 主线回归与性能收尾

## [V12] 2026-08-11 — Phase2 V1 实施 (gate 22/22)
