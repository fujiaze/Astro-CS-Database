# V15 Final Semantic Closure — 最终状态

日期：2026-08-14 ｜ 分支：main ｜ 控制包：AstroCS_Final_Semantic_Closure_Control_Package_V15.zip

## 冻结状态

```text
PHASE1_BASE_ALGORITHMS  = FROZEN（未触碰）
PHASE2_BASE_ALGORITHMS  = FROZEN
REJECTION_SEMANTICS     = FROZEN
WBPP_AUTO_POLICY        = FROZEN（profile=wbpp_current，WBPP 2.9.1 本机源码核准）
SATELLITE_REJECTION_GATE = PASS
BASE_API_CONTRACT       = FROZEN
PERFORMANCE_BASELINE    = FROZEN
FINALIZATION_SELF_REVIEW = PASS（6 轮 + clean-tree，见 self_review/）
```

## 验收门对照（V15 ACCEPTANCE_GATES.md）

| Gate | 状态 | 证据 |
| --- | --- | --- |
| G0 Baseline | PASS | 59/59 gate（含 V13 mosaic/geometry/UPM 回归）；浏览器 geometry truth PASS |
| G1 Rejection | PASS | RJ-001..008 全部修复并有回归测试；per-sample reason；typed params；eligibility/rejection 分层 |
| G2 WBPP | PASS | WBPP 2.9.1（PixInsight PCL 2.9.4）本机源码；bestRejectionMethod 政策；planning 层解析；default=auto |
| G3 Satellite | PASS | 20 exposure 受控注入 recall=1.0000；bias=0；n<=2 生产 run 100% UNDERDETERMINED |
| G4 Oracles | PASS | 硬编码路径清除（env 覆盖）；subprocess timeout；恒真断言修复；镜像改名 NOT_AN_ORACLE；边界矩阵 PASS |
| G5 Single-path | PASS | semantic_path_inventory.csv；浏览器第二套 HEALPix 映射已删除；duplicate production path=0 |
| G6 Interfaces/config/style | PASS | config_consistency_check.py PASS；schema/template/parser 单源；API 文档更新 |
| G7 Browser | PASS | 单一 DisplayTransformState；support linear；stretch-only 1.4-2.6ms；RAM 17MB 有界 |
| G8 Performance | PASS | sampler ~10min→9.2s；stage2 基准 3×；science 等价 |
| G9 Self-review | PASS | 6 轮 + clean-tree 终验 |
| G10 Final | PASS | known P0=0；known P1=0；failing core tests=0；duplicate production paths=0；semantic ambiguity=0 |

## 关键交付

- rejection 语义：10 个 canonical semantic ID + typed params + planning 层
  auto（WBPP 2.9.1）+ eligibility 分层 + per-sample reason + UNDERDETERMINED。
- 卫星线：真实生产门工具（20 exposure 受控注入）+ n<=2 生产诊断。
- 单路径：浏览器第二套 HEALPix 映射删除；config 默认单源；STF 单状态。
- 性能：sampler catalogue 全扫描 → dec 排序索引 + 帧 median 预计算；
  null-config 未初始化 bug 修复。

## 如实标注

- PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED。
- WBPP Auto 政策来自本机安装的 WBPP 2.9.1 源码（BPP-FrameGroup.js
  bestRejectionMethod），不是"凭记忆"。
- 卫星门 clean false reject：20 帧 linear_fit 在致密天区样本级
  ~27.6%（像素级 ≥1 拒绝 88.7%），为冻结 Siril linear-fit 语义在该参数
  下的固有行为；马赛克背景/星点 bias 均为 0（无净损伤）。
