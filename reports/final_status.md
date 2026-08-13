# V14 最终状态

```text
PHASE1_BASE_ALGORITHMS = FROZEN_CANDIDATE
PHASE2_BASE_ALGORITHMS = FROZEN_CANDIDATE
CROSS_STAGE_CONTRACTS  = FROZEN_CANDIDATE
BASE_API_CONTRACT      = FROZEN_CANDIDATE
PERFORMANCE_BASELINE   = FROZEN_CANDIDATE（baseline + 热点优化完成）
HIPS_BROWSER_BASE      = FROZEN_CANDIDATE
```

## 门状态

```text
G0 V13 baseline      = PASS（C/M 逐位等价；sampler 统计一致）
G1 pipeline audit    = PASS（实现/测试/文档/CLI 四者对应；47/47 合成门）
G2 data contracts    = PASS（DATA_SEMANTICS.md 唯一权威）
G3 UPM cleanup       = PASS（data=1 / geometry=1 / unobserved=39488；无 sentinel gauge）
G4 sampler truth     = PASS（8 tile × 56 污染点多 PSF/亮度/位置；recall 100%；false reject 0%；
                             连通 456/456；GC 真实覆盖：44096 几何控制点→33701 accepted，拒绝原因直方图+overlay）
G5 interface/config  = PASS（api_inventory + config consistency test + status/error ownership 文档化）
G6 browser           = PASS（Auto Global + Auto View + Lock/Reset STF + stretch-only redraw；
                             --stf-lock-probe PASS；10 分钟 soak：RAM 72→157MB 持平，LRU 4,624 淘汰）
G7 performance       = PASS（Phase1/Phase2/Browser 3 次 baseline；GC -20%、t4 -19%；
                             STF 重算 45.8ms / stretch-only 14.4ms；science 逐位等价）
G8 docs              = PASS（README 唯一入口 + 11 份 docs）
```

## 说明

- Phase1 本轮未触碰代码（审计确认），Phase2 仅 UPM component 统计语义
  修正（科学逐位等价）。
- 冻结候选以用户 V14 审核通过为最终生效。
