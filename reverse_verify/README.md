# reverse_verify —— 逆向验收工作区

> **主线做正向推导，本工作区做逆向验收。**
>
> 主线（`lib/` + `docs/` + `tests/`）从设计权威**正向推导**出实现；
> 本工作区**独立地**从第一性原理与公开文献/开源实现**逆向验证**主线声称的能力，
> 并把结果整理为**可发表的方案设计与定量实验**。

## 1. 定位与边界

| 维度 | 主线（正向推导） | 本工作区（逆向验收） |
|---|---|---|
| 权威来源 | `ASTROCS_DESIGN.md` → `AGENTS.md` → `ENGINEERING_SPEC.md` → … | **第一性原理 + 论文 + 开源实现** |
| 构建 | 根 `CMakeLists.txt`（`build/`） | **独立构建，不并入主线**（见 §4） |
| 产物 | 生产二进制 / 产品 | **实验代码 / 合成数据代码 / 报告 / 参考文献** |
| 对主线的态度 | 实现它 | **不采信它** —— 独立复现、独立判据、能红能绿 |

**硬边界**：
- 本工作区**不得**被根 `CMakeLists.txt` 引用；
- 本工作区**不产出**生产代码，**不改** `lib/`、`docs/`、`tests/`、`ci/`；
- 中间产物一律落 `run/reverse_verify/`（gitignore）；**本目录只放代码、报告、参考文献**。

## 2. 目录结构

```
reverse_verify/
├── README.md               # 本文件
├── CMakeLists.txt          # 独立构建（standalone project，不并入主线）
├── docs/                   # 方案设计与实验报告（论文雏形）
│   └── snr-propagation-design.md
├── references/             # 参考文献与开源项目（独立记录）
│   ├── README.md
│   ├── bibliography.md     # 逐条：标识 / 借鉴点 / 不借鉴点 / 理由
│   └── bibliography.bib    # BibTeX
├── experiments/            # 实验代码（C++ / Python 均可）
│   ├── p1_spatial_gain/    # Phase1 低阶空间乘法增益
│   └── snr_design/         # SNR 传播设计的支撑性数值估算
└── synthetic/              # 合成数据生成代码（可复用的真值场景）
```

## 3. 工作原则

1. **已知真值**：每个合成实验必须给出真值，并给**能红能绿的负例**（真值为「无效应」时度量必须归零）；
2. **独立复现**：不引用主线结论作为证据；需要时**自己算一遍**；
3. **判据先行**：先写判据与阈值，再看结果；不得事后放宽；
4. **诚实登记**：证据不足写「待定」并给判定方法，不臆断；
5. **可复跑**：脚本 + 数据生成 + 判据三件套齐备，任何人可复现。

## 4. 独立构建

```bash
# 独立构建（与主线 build/ 完全分离）
cmake -S reverse_verify -B run/reverse_verify/build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C run/reverse_verify/build
```

- 构建目录落 `run/reverse_verify/build`（gitignore），**不污染主线 `build/`**；
- Python 实验无需构建，直接 `python3 reverse_verify/experiments/<name>/<script>.py`；
- 如需链接主线静态库做对照，**显式指定路径**并在报告里写明版本（`git rev-parse HEAD`）。

## 5. 参考文献记录规则

**每条**必须包含：
1. **可核对标识**（DOI / arXiv 号 / 项目 URL + commit）；
2. **借鉴点**（我们采用它的什么）；
3. **不借鉴点**（它的什么不适用，为什么）；
4. **与我们场景的差异**（尺度 / 数据模型 / 假设）。

**禁止编造引用。** 无法核对来源的条目一律不收录。

## 6. 当前工作项

| 工作项 | 状态 | 报告 |
|---|---|---|
| Phase1 低阶空间乘法增益（`I_photo = k_photo·m(x,y)·I_cal`） | 进行中 | `docs/p1-spatial-gain.md` |
| SNR 全流程传播方案（单帧 SNR → 稀疏区域 SNR → Phase2 重建稠密 → 成品 SNR） | 进行中 | `docs/snr-propagation-design.md` |