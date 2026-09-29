# Data Flow

> 上游：docs/ASTROCS_DESIGN.md §8.2（阶段内：命名块内存管线与块生命周期）、§10（I/O 与原子产品）

本文写数据对象与载体：每个阶段消费什么、产出什么、以什么形态在节点之间与阶段之间流动。
节点流程的规范序在最高设计 §4.2（normalize）/ §5.2（mosaic）/ §6.2（export），本文不复制节点链。

## 载体

| 载体 | 范围 | 形态 | 规范依据 |
| --- | --- | --- | --- |
| 命名块（`PipelineFrame`） | 阶段内节点之间 | 内存对象，带冻结元数据（名字/形状/类型/单位/可缺性/生产者/消费者/生命周期） | `docs/engineering/PIPELINE_BLOCK_CONTRACT.md` §1、§2 |
| HiPS 产品树 | 跨阶段 | 磁盘目录 + manifest + 哈希 | 最高设计 §8.1、§10 |
| 运行配置 | 阶段入口 | JSON（块列表形态，每块一块级 `output_dir`） | 最高设计 §3.2、§4.3 |

## Phase1（normalize）

| 方向 | 数据对象 | 形态 |
| --- | --- | --- |
| 输入 | 亮场帧 + 母版校准帧（bias/dark/flat） | FITS/XISF，经 `aio` 唯一 I/O 边界读入 |
| 输入 | 运行配置（滤镜、精度模式、落盘形态、块级 `output_dir`） | JSON |
| 中间 | 校准像素面、有效/坏点掩膜、背景与噪声面、权威 WCS、星点绑定行、PSF 模型、测光归一化后的像素面、帧级与稀疏 SNR | 阶段内命名块 |
| 输出 | 逐帧 HiPS 产品树（signal / support / 可选 variance、ivar / 可选稀疏 SNR 标准层） | 磁盘产品 + manifest + 哈希 |
| 输出 | 结构化 JSON（声明产物路径与必需字段，符合 Phase2 输入格式） | JSON |

- 该阶段的唯一用户命令 = `normalize --json <config.json>`（最高设计 §7.1 命令树）。
- 测光输出语义：产物通量处于逐帧相对测光零点的线性面亮度，测光归一化在 `measure_flux` 同一步内施加到像素（最高设计 §4.4）。

## Phase2（mosaic）

| 方向 | 数据对象 | 形态 |
| --- | --- | --- |
| 输入 | 一组合同兼容的 Phase1 HiPS 产品树 + 其运行清单 | 磁盘产品 |
| 中间 | coverage 重叠图（MOC union, target_order）、控制采样点与 `control_ivar`、公共天光面 `B_ref` 与逐帧偏差 `δ_k`、归一化后的样本面、逐像素排异结果与服务支撑度 | 阶段内命名块 |
| 落盘（可选） | UPM 模型稀疏 JSON（经 `aio_upm` 读写）、稠密缓存 | 磁盘产品 |
| 输出 | 马赛克 HiPS 产品树 + 运行清单 | 磁盘产品 + manifest + 哈希 |

- 该阶段的唯一用户命令 = `mosaic --json <config.json>`（最高设计 §7.1 命令树）。
- UPM 的模型语义（加性天光面 + 逐帧偏差、`calibrated = raw − δ_k`、保留公共天光面）
  正本 = `docs/detail/algorithms_phase2/11_upm.md` 与 `docs/science/PHASE2_UPM.md`。
- 排异与集成的数据对象语义（逐像素按几何可贡献帧数路由、逆方差加权、support 归约）
  正本 = `docs/detail/algorithms_phase2/12_rejection.md` §9 与 `docs/detail/algorithms_phase2/13_integration.md`。

## Phase3（export）

| 方向 | 数据对象 | 形态 |
| --- | --- | --- |
| 输入 | 马赛克 HiPS 产品树（只读） | 磁盘产品 |
| 中间 | properties 读取结果、目标投影 WCS、重采样后的子块 | 阶段内命名块 |
| 输出 | 平面 FITS 产品（含 VARIANCE/IVAR 扩展 HDU） | 磁盘产品 + 校验报告 |

- 该阶段的唯一用户命令 = `export --json <config.json>`（最高设计 §7.1 命令树）。

## 完成判定

每个阶段的输出以运行完成清单为交付判据，机器可核入口 = `doctor --run-manifest <manifest.json>`。

## 数据契约

见 `docs/science/DATA_SEMANTICS.md` 与 `docs/TRACEABILITY.csv` 的 `DATA-*` 行。
