# per-doc: docs/science/CALIBRATION.md（SCI-CAL-001，FROZEN）

## 发现

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| R2-B-01 | 143-144（§9）、§9a | "不传播母版方差至 cal 的方差项" | 与 ASTROCS_DESIGN.md:197（§3.6 硬约束"校准方差传播、共享 master 相关性"）及 PHASE1_DETAILED_DESIGN.md:52-58（必须物理传播）互斥 | CONTRADICTS-DESIGN | DESIGN-OK-DOC-WRONG |

## 处置建议

- §9/§9a 改为：本层必须传播 master 方差与共享相关性（共同 master_id / 低秩 / 相关核）；ivar 由噪声模型在 cal 方差之上补充，而非替代。
- 共享 master 误差在 Phase2 不会平均掉（P1-A 复核），故"不传播"会导致权重过度乐观——这是科学后果，需在文档写明。
- 实现侧：lib/algorithms/calibration/src/v6_calibration_covariance.cpp 已按正确系数实现，但 V6_CALIBRATION_COVARIANCE.md 自述"未接线 Phase session"；请前台确认生产 DAG 是否消费（R2-B-03）。
