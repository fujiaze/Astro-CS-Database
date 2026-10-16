# Astro Celestial Sphere Database

ACSD 将天文 CCD/CMOS 观测校准为具有明确坐标、光度尺度和来源链的球面 HiPS，合成马赛克，并导出普通平面 WCS FITS。科学内核面向 Windows amd64 与 Linux amd64，使用 CPU；Windows Qt6 GUI 独立构建。

## 三个命令

```text
acsd normalize --json <config.json>
acsd mosaic    --json <config.json>
acsd export    --json <config.json>
acsd <command> --template [-o <path>]
acsd <command> --help
```

`normalize` 逐帧完成校准、天体测量、源与 PSF 建模、测光标度、条件噪声和球面映射，输出单帧 signal HiPS 及科学辅助数据。`mosaic` 消费已发布帧产品，估计相对背景、排异并合成为一个 signal HiPS。`export` 将兼容信号 HiPS 重采样为二维 WCS FITS。三个命令独立运行，通过磁盘产品与 manifest 交换数据。

运行的 stdout 是 JSONL 事件流，配置、事件与退出码见[CLI 协议](docs/engineering/contracts/CLI_PROTOCOL.md)。不同设备和通带由用户选择组合，实际响应与系统差异随产品解释。条件参考 SNR、面亮度方差与背景控制方差各有定义，见[统一科学模型](docs/science/unified/UNIFIED_SCIENCE_MODEL.md)。

## 构建与开发

依赖、平台工具链和 CMake 选项见[构建说明](docs/engineering/build/BUILD_GRAPH.md)及[依赖规范](docs/engineering/standards/DEPENDENCY.md)。在依赖就绪的工具链环境中，基本配置命令为：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

GUI 见[GUI 设计](docs/engineering/gui/GUI_DESIGN.md)。版本来自根目录[VERSION](VERSION)，安装与发行见[发布说明](docs/engineering/build/RELEASE.md)。

## 阅读与源码

从[文档入口](docs/README.md)进入[最高设计](docs/ACSD_DESIGN.md)，再读科学、工程与模块说明。目标能力与实际实现集中对照于[实施差异](docs/engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)，静态接线不代替真实观测验证。

| 目录 | 内容 |
|---|---|
| `lib/algorithms/` | 校准、天测、PSF、测光、噪声、Drizzle、背景与重采样算法 |
| `lib/infrastructure/` | CLI、Runtime、调度、统一 I/O 与可观测性 |
| `lib/include/acsd/` | 公共类型与接口 |
| `eng/` | 机器合同、独立测试、构建和打包 |
| `docs/` | 正式科学、工程与模块设计 |
| `实验/` | 研究、失败反例、脚本与可复核记录 |

协作按[AGENTS.md](AGENTS.md)执行。文档整理的[范围与处置](实验/documentation-refactor/REPORT.md)和[待推导问题](实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)单独保留；文档修订不代表端到端科学验收。
