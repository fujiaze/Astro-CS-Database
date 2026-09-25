# Data Flow

> 上游：ASTROCS_DESIGN.md §8（软件架构）

> 本文按最高设计 §4.2（normalize 节点流程）/ §5.2（mosaic 固定科学流程）/ §5.5（逐像素排异）/ §8.1-§8.2（阶段内命名块内存管线与块生命周期）描述；入口 = 唯一 CLI 的 `normalize` / `mosaic` / `export` 三个子命令。

## Phase1（normalize：单帧管线）

```text
FITS/XISF 亮场 + 母版
  → aio read（唯一 I/O 边界）
  → calibration（bias/dark/flat） → cosmetic/validity（坏点/宇宙线）
  → background/noise（背景与噪声）
  → platesolve（星表匹配 + 稳健迭代精化 WCS = 权威 WCS）
  → star_detection（星表引导检测：以本帧权威 WCS 逆投影 Gaia；一次检测、一次通量积分，三处复用）
  → psf（PSF 建模）
  → photometry（通量定标；并在**同一步**内把测光归一化施加到像素，不设独立节点）
  → noise_snr（噪声/帧级 SNR）
  → drizzle（球面重采样/方差传播）
  → 产品验证 → 原子发布 HiPS（signal/support[/variance/ivar] + JSON）
```

- 入口 = 唯一 CLI 的 `normalize` 子命令（最高设计 §7.1 命令树）。
- 阶段内节点之间**传内存块**（`PipelineFrame` 命名块），**不落中间文件**（最高设计 §8.1:502 / §8.2:526-527）；
  当前生产实现仍用磁盘 JSON/FITS 传递，属**现行设计缺口**（生产节点覆盖率 **1/20**、相邻节点传块 **0**）。

## Phase2（mosaic：多帧统一模型）

```text
多帧 Phase1 HiPS
  → admit（兼容性校验） → coverage（MOC union, target_order）
  → sampling（控制采样 + patch estimator）
  → upm（Huber IRLS + 控制点权重 + 弱零锚 + 连通分量；纯加性相对模型）
  → upm persist（sparse JSON via aio_upm；dense cache 可选）
  → block plan / upm apply（每帧 frame_id → δ_k(frame, leaf)；**默认只扣偏差、保留公共天光面**）
  → rejection（**逐像素按几何可贡献帧数 N 自动选择**：`1≤N≤3` none / `4≤N≤5` percentile /
      `N≥6` winsorized（M3：原 `N≥16` linear fit 档改投）；映射表见 `ASTROCS_DESIGN.md` §5.5；
      禁 min/max —— 表落 `docs/plugins/algorithms_phase2/12_rejection.md` §9，**只引用**）
  → integration（加权均值 + support reducer）
  → 产品验证 → 原子发布马赛克 HiPS + verify
```

- 入口 = 唯一 CLI 的 `mosaic` 子命令（最高设计 §7.1 命令树）。
- 排异**不是「7 种任选」**：**逐像素按 N 自动选择**，冻结映射表落位
  `docs/plugins/algorithms_phase2/12_rejection.md` §9（权威 = `ASTROCS_DESIGN.md` §5.5 / `docs/science/REJECTION.md`；**只引用，不复制**）。

## 数据契约

见 docs/contracts/DATA_SEMANTICS.md 与 docs/TRACEABILITY.csv DATA-* 行。
