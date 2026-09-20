# AstroCS Pipeline（Phase1 → Phase2）

> ⚠ **DOC-202 订正（R15 / R16 / R17 / R21 / R22，2026-09-20）**：本文按最高设计
> §3.2 / §4.2 / §4.5 / §6.2 / §7.1a 重写。
> **入口只有一个**：`ACSD Cli` / `acsd_cli` 的 `normalize` / `mosaic` / `export` 三个子命令
> （最高设计 §6.2）。旧 `orchestrator.exe` / `astrocs-stage2` **不是入口**
> （最高设计 §7.1/§11「旧可执行程序不是入口」），已从本文入口位删除（仅在本注留痕）。
> **阶段内走内存块管线，阶段间落盘**（最高设计 §7.1a）。

## Phase1（normalize：单帧 → 单帧 HiPS）

```text
ingest（输入 + 元数据校验） → calibration（bias/dark/flat）
  → cosmetic/validity（坏点/宇宙线） → background/noise（背景与噪声）
  → platesolve 第一轮（盲解粗 WCS：为星表投影提供近似坐标）
  → star_detection（星表引导检测：用第一轮 WCS 反向投影 Gaia，只拟合星表位置）
  → psf（PSF 建模）
  → platesolve 第二轮（高纯度星表精解 WCS = 权威 WCS）
  → photometry（测光/通量定标）
  → apply photometry（I_photo = k_photo·m(x,y)·I_cal 落到像素）
  → noise_snr（噪声/SNR/帧级 SNR） → drizzle（HEALPix NESTED）
  → 产品验证 → 原子发布 HiPS + JSON
```

- **两轮 WCS 是强制节点**（最高设计 §3.2）：第一轮盲解**只为星表投影提供近似坐标**，
  其星表**不是**权威科学产品；第二轮精解结果才是权威 WCS。
- **`apply photometry` 是强制节点**（§3.2 / §7.1「生产调度面必须覆盖强制节点」）：
  测光归一化**必须真正落到像素**；未启用时产品必须显式记 `degraded_reason` 并 fail-closed，
  **不得**按未归一化 ADU 静默走完全链。
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
  **冻结映射表（EXP-204 定案 + WBPP 一手实测）**：
  `1≤N≤3` **none（不排异）** / `4≤N≤5` percentile / `6≤N≤15`（或 BIAS/DARK）winsorized /
  `N≥16` linear fit；**禁止 min/max**。
  冻结表落位 = `docs/plugins/algorithms_phase2/12_rejection.md` §9（**只引用，不复制**）。
- **阶段内节点之间传块，不落中间文件**（最高设计 §7.1a）；当前生产实现仍用磁盘
  JSON/FITS 在阶段内传递，属**已登记缺口**（迁移归 FIX 代码任务）；
  **未完成前不得声称本条款已满足**。

## 关键不变量

- **科学冻结权威 = `docs/science/`（公式）与 `docs/algorithms/`（推导）**
  （最高设计 §0.1/§1.1）。~~原引 `SCIENCE_FREEZE.md` **不存在**，已作废~~（R22）。
- 序列化：V11（外部 oracle 冻结）；sampler/UPM：V13/V14（历史内部指代）。
- Browser 不拥有科学数据解释权（**工具分类，非发布**；最高设计 §7.1/§10.1）。
