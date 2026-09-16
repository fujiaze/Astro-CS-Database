# AstroCS 插件文档集（docs/plugins）

---

## 1. 模块归属

```text
docs/plugins/
├── 00_INDEX.md                 本文
├── algorithms_phase1/          normalize 相关科学模块（8 篇）
├── algorithms_phase2/          mosaic 相关科学模块（5 篇）
├── algorithms_phase3/          export 相关科学模块（3 篇）
└── infrastructure/             基建模块（7 篇）
```

**代码落位**：所有科学算法模块在 `lib/algorithms/` 下**并联放置**；`phase1/2/3` 是设计层面的内部指代，代码目录统一为 `lib/algorithms/` 平铺 + `lib/infrastructure/cli/{normalize,mosaic,export}`，引用对应算法模块。（见最高设计 §7.1）

---

## 2. 模块总表（23 篇）

> 分组标题中的 phase1/2/3 仅是**阅读分组**（文档目录），代码中所有算法模块在 `lib/algorithms/` 下并联放置。

### algorithms/phase1（normalize，8）

| 文档 | 模块 | 一句话职责 |
|---|---|---|
| `01_calibration.md` | calibration | 减偏置/暗流/平场，单位与方差传播 |
| `02_cosmetic.md` | cosmetic | 坏点/热像素/宇宙线修正 |
| `03_star_detection.md` | star_detection | 源探测与质心/矩 |
| `04_psf.md` | psf | 空间 PSF 建模与参数化 |
| `05_platesolve.md` | platesolve | 天体测量解算与 WCS 拟合 |
| `06_photometry.md` | photometry | 孔径/PSF 测光与通量定标 |
| `07_noise_snr.md` | noise_snr | 噪声模型、variance/ivar、SNR、depth、信息权重、帧级 SNR（文件头）、稀疏层 |
| `08_drizzle.md` | drizzle | 球面 Drizzle / HEALPix 累积 |

### algorithms/phase2（mosaic，5）

| 文档 | 模块 | 一句话职责 |
|---|---|---|
| `09_coverage.md` | coverage | 覆盖联合与几何有效域 |
| `10_sampling.md` | sampling | 控制点采样（避开亮星/异常） |
| `11_upm.md` | upm | 加性背景/梯度统一模型拟合与施加 |
| `12_rejection.md` | rejection | 排异算法与自动策略 |
| `13_integration.md` | integration | 扩展源 GLS / 点源 Q-W / psfsw_robust |

### algorithms/phase3（export，3）

| 文档 | 模块 | 一句话职责 |
|---|---|---|
| `14_projection.md` | projection | 投影 registry（内置 TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA） |
| `15_resample.md` | resample | 球面反向映射与重采样 |
| `16_fits_output.md` | fits_output | 流式 FITS + WCS/coverage/validity |

### infrastructure（7）

| 文档 | 模块 | 一句话职责 |
|---|---|---|
| `17_aio.md` | aio | FITS/HiPS/manifest 唯一 I/O、原子提交 |
| `18_cli.md` | cli | 命令解析、JSON/JSONL、取消、退出码 |
| `19_runtime.md` | runtime | typed DAG、调度、线程预算、资源监控 |
| `20_benchmark.md` | benchmark | CPU profile 生成与校验 |
| `21_observability.md` | observability | 日志、事件、运行图 |
| `22_gaia_xpsd_client.md` | gaia_xpsd_client | 外部星表查询、缓存、坐标/历元语义 |
| `23_hips_browser.md` | hips_browser | 未来 GUI 可视化组件（不进产品 manifest） |

---

## 3. 插件文档统一模板（每篇 8 节）

每篇插件文档固定 8 节（机器与人都按此结构消费）：

1. **职责与边界** —— 一句话职责 + "不是做什么"；
2. **权威依据** —— 最高设计节号 + docs/science + docs/algorithms 具体引用；
3. **输入/输出数据合同** —— 引用 contracts/schemas，不复制 schema；
4. **算法与公式要点** —— 关键公式（引用权威推导，不重复展开）；
5. **配置项** —— phase_config 字段、默认值、单位、约束；
6. **接口/ABI** —— entrypoint、端口、所有权；
7. **错误与边界** —— 退出码、fail-closed、边界情况；
8. **测试与 Oracle** —— 必须有的测试与独立 Oracle。

---

## 4. 每篇文档必须回答的问题

- 本模块的**输入对象**和**输出对象**分别是什么（用 `docs/design/UNIFIED_MODEL.md` 的术语）？
- 本模块**不做什么**（边界，防止越界改科学）？
- 本模块的**验收门**是什么（可复跑命令）？
- 本模块的**配置**哪些字段、默认值、单位？
- 本模块与相邻模块的**DAG 位置**？

---

## 5. 维护规则

- 插件文档与代码/合同同步更新；改合同必须先改插件文档（文档先行）；
- 插件文档不得与最高设计冲突；冲突以最高设计为准并修订本文档；
- 新增模块 = 新增插件文档 + module.yaml 注册 + 测试；删除模块 = 反向操作并登记。
