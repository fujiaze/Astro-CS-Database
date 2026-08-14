# Round 0 — Contract / Scope Review（V15 Final Semantic Closure）

日期：2026-08-14 ｜ 分支：main（HEAD dbb3eec）｜ 控制包：AstroCS_Final_Semantic_Closure_Control_Package_V15.zip（SHA256SUMS 见 run/temp/AstroCS_Control_V15/SHA256SUMS.txt）

## 1. 当前唯一 production entry

| 阶段 | 入口 | 说明 |
| --- | --- | --- |
| Phase1 | `orchestrator.exe <stage1.json>`（lib/orchestrator/cpp） | 单帧→单帧 HiPS（signal/support/SNR） |
| Phase2 | `astrocs-stage2.exe <stage2.json>`（lib/phase2/tools/stage2.cpp） | 多帧→马赛克 HiPS |
| Browser | `healpix_browser_qt.exe`（lib/healpix_db/healpix_browser_qt） | 只消费 HiPS，不拥有科学解释权 |

Python 仅保留 `NON_PRODUCTION_TOOL_ONLY` 标记的测试/研究脚本，禁止进入生产链。

## 2. 科学语义（冻结基线）

- HiPS 几何/序列化/hierarchy：V11 外部 oracle（Hipsgen）冻结；NESTED 唯一；leaf_order=tile_order+9；FITS index=(511-x)*512+y。
- background-clean sampler / standardized Huber / smooth continuation：V13（用户 ACCEPTED）。
- UPM component 语义：V14（data/geometry/unobserved 分开）。
- signal=科学表面亮度，负值保留；support∈[0,1]，support=0 无覆盖；invalid=NaN 或 support<=0；有效样本=finite && support>0。
- frame_id=`p2_frame_id(path)`（FNV-1a 64，payload 敏感，与顺序无关）；input_manifest_hash 稳定摘要。
- 权重：weight_mode=auto → support_x_snr2（R5 冻结；SNR²×support）；local SNR 优先，缺失才回退整帧 median，禁止 1.0-as-unknown。

## 3. public ABI（本轮可改，受控）

- `p2_*` C ABI（extern "C" 不抛异常）；返回码 0=OK，非 0 语义由各头文件独占定义；`err` 缓冲只承载日志文本。
- 本轮新增/修改：
  - `p2_reject_plan_resolve`（auto 在 planning 层解析为显式方法 + typed params）；
  - `p2_eligibility_filter`（资格层：finite/valid/support/quality → CandidateStack）；
  - `p2_reject_stack_ex`（只对 CandidateStack 执行显式方法；per-sample reason；UNDERDETERMINED status）；
  - `p2_rejection_workspace_*`（可复用 scratch，避免每像素堆分配）。
  - `p2_reject_stack`（旧签名）保留为 COMPAT adapter，仅测试/旧调用；生产 Stage2 不再调用。
- 枚举数值保持兼容（P2_REJECT_* 0..10 不变；P2_REJECT_SIGMA=1 的 canonical semantic id 为 `astrocs.robust_mad_clip.v1`）。

## 4. on-disk contract

- HiPS：IVOA 1.4（signal/support/snr 产品，NESTED，512 tile）。
- UPM：`astrocs-upm-v1` JSON sparse + dense cache（checksum 校验）。
- manifest.json / diagnostics.json / controls_accept.json。
- `run/` 是唯一运行输出目录；testdata/ 只读；禁止向 testdata 写运行产物。

## 5. config defaults 唯一来源（V15 将统一）

- Stage2：`lib/phase2/include/astro/phase2/stage2_common.h`（struct 默认）↔ `stage2_common.cpp`（parser）↔ `工程控制/schemas/stage2.schema.json` ↔ `工程控制/configs/stage2.template.json` ↔ `docs` ↔ `tools/config_consistency_check.py`。
- **V15 变更**：production 默认 `rejection.method=auto` + `profile=wbpp_current`（不再 winsorized_sigma）；rejection 参数改为 method-specific typed（不再共享 low/high/max_iterations）；旧 low/high/max_iterations 仅作 deprecation adapter。

## 6. WBPP Auto 政策（本机源码证据，V15 核准）

- PixInsight 安装：`C:\Program Files\PixInsight`（PCL 2.9.4，2025-03-31）。
- WBPP 2.9.1（Released 2026-01-16T11:44:34Z），源码 `C:\Program Files\PixInsight\src\scripts\BatchPreprocessing\`。
- Auto 路由（BPP-FrameGroup.js `bestRejectionMethod()`）：
  - `n < 6` → PercentileClip；
  - `6 <= n <= 15`（或 BIAS/DARK）→ WinsorizedSigmaClip；
  - `n > 15` → LinearFit。
- Auto 在 integration group/stack 层解析一次（BPP-processing.js `doIntegrate`），**不在 pixel loop 内按 effective count 路由**。
- 本机 profile 冻结为 `wbpp_current`；PIXINSIGHT_EXACT_COMPATIBILITY=NOT_CLAIMED（无法证明与 PixInsight 内核 bit-exact）。

## 7. rejection canonical semantic IDs（V15）

| semantic_id | display | 参数（typed） | minimum N | oracle/reference |
| --- | --- | --- | --- | --- |
| astrocs.none.v1 | none | — | 0 | — |
| astrocs.robust_mad_clip.v1 | sigma（alias） | lower_sigma/upper_sigma/max_iterations | 3 | Astropy sigma_clip(cenfunc=median, stdfunc=mad_std) |
| astrocs.winsorized_sigma_siril_1_4_3.v1 | winsorized_sigma | lower_sigma/upper_sigma/max_iterations | 3 | Siril 1.4.3 rejection_float.c（ORACLE ONLY） |
| astrocs.avsigclip_iraf.v1 | averaged_sigma | lower_sigma/upper_sigma/max_iterations | 3 | IRAF AVSIGCLIP |
| astrocs.linear_fit_siril_1_4_3.v1 | linear_fit | lower/upper/max_iterations | 4 | Siril 1.4.3 harness（未修改源码） |
| astrocs.generalized_esd_nist.v1 | generalized_esd | alpha/max_outliers | 3 | NIST/Rosner 54 点 |
| astrocs.rcr_2_4_7_ss_median_dl.v1 | rcr | technique=ss_median_dl | 3 | 官方 rcr 2.4.7 固定版本（ORACLE ONLY） |
| astrocs.percentile_siril.v1 | percentile | low_fraction/high_fraction | 2 | Siril percentile 语义 |
| astrocs.median_std_clip.v1 | median_sigma | lower_sigma/upper_sigma/max_iterations | 3 | WBPP median+SD |
| astrocs.minmax.v1 | minmax | reject_low_count/reject_high_count/max_iterations/min_kept | 5 | WBPP Min/Max |

## 8. eligibility / rejection 分层（V15 强制）

```text
Raw Contributors → EligibilityPolicy(finite/valid/support/quality) → CandidateStack
→ RejectionPlan(Auto 在 planning 层解析) → RejectionKernel → RejectionDecision[reason]
→ WeightedIntegration
```

auto 的 nominal contributors = 该 tile/cohort 几何上可贡献的独立 exposure 数（stage2 用 tile 覆盖帧数 depth），禁止用当前 pixel effective count 路由。effective < method minimum N 或 <= underdetermined_n（默认 2）→ `REJECTION_UNDERDETERMINED`（可全接受但必须记录，禁止偷偷换算法）。

## 9. 卫星线真实生产门（V15）

- 先审计 exposure granularity：Phase2 输入必须是 per-exposure HiPS（Phase1 每帧独立产物），rejection 前不得 collapse 成 panel/master。
- 门：>=15–20 独立 exposure（真实 T4 Victory_Nebula Lum 同夜同区裁剪帧）+ 1 帧受控注入卫星线 + production Phase2 auto。
- n=1–2 必须标 `REJECTION_UNDERDETERMINED`，不得宣称可剔除卫星线。

## 10. 允许删 / 禁止动

- 允许：重复 production 实现删除/合并（保留 CPU reference + ACR backend 同一 contract）、ABI 兼容 wrapper、test-only oracle、stale code/comment、重复常量/默认值单源化。
- 禁止：healpix_stack（Stage2 冻结模块）、Phase1 冻结算法、HISS deprecated 链（可保留 legacy verify）、ACR 业务算法/OpenMP/Pipeline/CLI、外部数据目录删除、testdata 写入。
- Browser：support 固定 linear [0,1]，不走 signal STF；STF 变化禁止重新 sky projection / HEALPix sampling / FITS decode。

## 11. benchmark / truth datasets

- Phase2 t4 overlap（t4_crop_v3.hips × t4_full_v3_final.hips，n=2 重叠区）+ GC 3-panel（v7）。
- 合成 rejection matrix（N=2..500 × 污染种类）；NIST Rosner 54；RCR official mask {6,7}；Siril linear_fit 固定向量。
- 卫星门：Victory_Nebula Lum 20 帧 1024² crop。

## 12. 语义仍模糊点（已按上述决策冻结，无 STOP）

无。所有 V15 要求均可依据控制包 + 本机 WBPP 源码 + 现有代码/文档判定。

## Round0 结论

`ROUND0=PASS`。冻结上述合同后进入实现。
