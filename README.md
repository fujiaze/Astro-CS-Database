# AstroCS

AstroCS 是一个原生天文图像处理内核（C++/DLL），将真实 FITS 图像转换为标准化 HEALPix 球面帧数据库（HISS），并通过 SNR² 加权全局加性梯度统一、稳健排异和多帧叠加生成连续球面数据库（HCSD）。

## 当前状态

- 当前阶段：Phase1 架构纠正（R10）——**尚未整体闭合**
- 正式科学运行只有唯一入口：`orchestrator.exe <stage1.json>`
- 已完成：JSON 唯一入口与严格 Schema、生产 Python 封装删除、全链路 FP32/FP64 双精度、合成数据与单帧逐 Gate 验证
- 未完成：710 帧全量回归、Stage2 梯度/叠加、FAST 研究、ACR 接入

## 权威文档

项目唯一权威文档维护在 GitHub Wiki，本 README 仅作为入口与导航：

- https://github.com/fujiaze/Astro-CS-Database/wiki

## 构建

统一工具链唯一入口是 `toolchain.ps1`（环境/自检/编译/运行/审核包），规范与坑点见 [AGENTS.md](AGENTS.md) 的“统一工具链”一节。快速开始：

```powershell
.\toolchain.ps1 check
.\toolchain.ps1 build
```

## 唯一运行示例

```powershell
.\lib\orchestrator\cpp\orchestrator.exe run\temp\r10_validation\fp32\stage1.json
```

所有输入、参数、精度、输出与日志位置全部来自该 JSON 文件。Schema 与模板见 `lib/orchestrator/configs/`，或运行 `orchestrator.exe --print-schema`。

## 仓库结构

- `lib/` — 项目源码（C++ 模块，唯一修改代码处）
- `工程控制/` — 工程规范、任务清单、证据
- `testdata/` — 只读原始数据（710 帧 + 校准母版）
- `run/` — 唯一运行输出目录（日志、校准、Drizzle、HISS）
- `AstroCS.wiki/` — 本地 Wiki 克隆
