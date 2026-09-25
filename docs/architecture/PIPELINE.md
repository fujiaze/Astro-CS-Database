# Astro Celestial Sphere Database（ACSD） Pipeline（Phase1 → Phase2）

> 上游：ASTROCS_DESIGN.md §8（软件架构）

> 本文按最高设计 §3.2 / §4.2 / §4.5 / §6.2 / §8.2 描述。
> **入口只有一个**：`acsd.exe`（Windows）/ `acsd`（Linux）的 `normalize` / `mosaic` / `export` 三个子命令
> （最高设计 §7.1）。
> **阶段内走内存块管线，阶段间落盘**（最高设计 §8.2）。

## Phase1（normalize：单帧 → 单帧 HiPS）

```text
ingest（输入 + 元数据校验） → calibration（bias/dark/flat）
  → cosmetic/validity（坏点/宇宙线） → background/noise（背景与噪声）
  → platesolve（星表匹配 + 稳健迭代精化 WCS = 权威 WCS）
  → star_detection（星表引导检测：以本帧权威 WCS 逆投影 Gaia，只拟合星表位置）
  → psf（PSF 建模）
  → photometry（测光/通量定标；并在**同一步**内把 I_photo = k_photo·m(x,y)·I_cal 施加到像素）
  → noise_snr（噪声/SNR/帧级 SNR） → drizzle（HEALPix NESTED）
  → 产品验证 → 原子发布 HiPS + JSON
```

- **WCS 解算只有一个节点、一个权威解**（最高设计 §4.2）：近似指向由 `wcs.init_source`
  （`header_pointing` / `config` / `neighbor_crval`）给出，不是独立的解算节点；
  `platesolve` 在该指向下完成星表匹配与稳健迭代精化，其输出即唯一权威 WCS。
  解算轮次数是求解器实现细节，不是流程语义。
- **节点序与依赖边**（最高设计 §4.2）：`platesolve` 按帧读校准后像素自行做星点检测与星表匹配，
  **不消费** `star_detection` 的产物；而权威检测的星表逆投影需要**含取向**的完整 WCS（取自本帧解算产物）
  ⇒ 解算在检测与 PSF 建模之前。节点序 = 注册表端口图 DAG 的拓扑序，且与注册表声明序一致
  （机器判据见 `docs/contracts/PIPELINE_BLOCK_CONTRACT.md` §7.1）。
- **测光归一化施加是 `photometry` 节点内的强制步骤**（§3.2 / §7.1「生产调度面必须覆盖强制节点」）：
  测光归一化**必须真正落到像素**——施加与拟合**同一步**完成（省一次中间产物落盘 = 省一次写 + 一次读的 IO 往返），
  生产 IR 的 normalize 阶段因此是 8 节点（`calibrate/cosmetic_correct/detect_sources/plate_solve/measure_flux/estimate_snr/drizzle_stack/write_hips`），
  施加是 `measure_flux` 的内部步骤而非第 9 个节点；未启用时产品必须显式记 `degraded_reason` 并 fail-closed，
  未归一化 ADU 的路径终止于显式 `degraded_reason`。
- 入口：`normalize --json <config.json>`（唯一 CLI 子命令）。

## Phase2（mosaic：多帧 → 马赛克 HiPS）

```text
admit（兼容性校验） → coverage（重叠图 union） → sampling（控制采样）
  → upm（纯加性相对模型 raw − δ_k，保留公共天光面 B_ref；g_k≡1 本期不启用）
  → upm apply（归一化） → rejection（排异推断） → integration（科学目标集成）
  → 产品验证 → 原子发布马赛克 HiPS
```

- 入口：`mosaic --json <config.json>`（唯一 CLI 子命令）。
- **排异不是「7 种任选」**：排异算法**逐像素按该像素几何可贡献帧数 N 自动选择**；
  **排异档位映射表（档界取自 WBPP 2.5.9，逐项正本见 §9）**：
  `1≤N≤3` **none（不排异）** / `4≤N≤5` percentile / `N≥6` winsorized
  ；
  档位取值限于 percentile/winsorized/linear fit（linear fit 仍为对照档 `wbpp_2_9_1` 的 `N>15` 档）。
  冻结表落位 = `docs/plugins/algorithms_phase2/12_rejection.md` §9（**只引用，不复制**）。
- **阶段内节点之间传块，不落中间文件**（最高设计 §8.2）；当前生产实现仍用磁盘 JSON/FITS
  在阶段内传递，属**现行设计缺口**：20 个生产节点中命名块管线覆盖率 = **1/20**（唯一命中是
  drizzle 节点内自建自毁），相邻节点之间传块 = **0**；本条款的满足声明以覆盖率达成为条件。

## 关键不变量

- **科学冻结权威 = `docs/science/`（公式）与 `docs/algorithms/`（推导）**
  （最高设计 §0.1/§1.1）。
- 序列化：V11（外部 oracle 冻结）；sampler/UPM：V13/V14（内部指代）。
- Browser 不拥有科学数据解释权（**工具分类，非发布**；最高设计 §7.1/§10.1）。
