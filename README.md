# AstroCS

AstroCS 是一个原生天文图像处理内核（C++/DLL），将真实 FITS 图像转换为标准化
HEALPix 球面帧数据库（IVOA HiPS，生产末端；HISS 仅作迁移对账），并通过
SNR² 加权全局加性梯度统一、稳健排异和多帧叠加生成连续球面数据库（HCSD）。

## 当前状态

- 当前阶段：**Phase1 Final Signoff V5 code freeze candidate = PASS**（2026-08-09，
  控制包 `AstroCS_Phase1_Final_Signoff_V5_HiPS_Layout_Precision`；HiPS 标准
  tile 排列经 CDS Hipsgen MAPTILES 逐像素 Oracle 冻结，SNR 精度与生产配置
  收敛完成；Dataset-domain freeze = WAITING_T1，仅缺 T1 真实数据，外部阻塞）
- 正式科学运行只有唯一入口：`orchestrator.exe <stage1.json>`
- 已完成：JSON 唯一入口与严格 Schema、HISS signal FP32/FP64 数据面、
  候选零漏选（9003/9003）、科学矩阵、Sphere→Plane 双向底层、
  PSF 坐标契约、XPSD 官方解码（PCL fluxMin/fluxMul）、HiPS 直写生产链
  （CFITSIO 4.6.4，signal/support/snr 3 子产品 + hierarchy + MOC）
- Phase1 生产末端：单帧平面 → HEALPix 球面 → HiPS（HIPS_VERIFY）；
  HISS 正式 deprecated（仅 `validation.legacy_hiss_compare=true` 写出）；
  生产默认 pixfrac=0.8（省略即 0.8）、`output.hips` 唯一正式输出；
Phase2 = 球面马赛克（多帧 HiPS → UPM → 动态分块 → 排异叠加 → HiPS Mosaic；不再使用 HICS）；Phase3 = 球面→平面导出（底层双向已冻结）
- 未完成（不在本轮）：Stage2 / FAST / ACR、710 帧全量回归
- 严格科学验收以**合成真值数据**为主；真实 testdata 仅辅助/性能/集成验证

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
- `run/` — 唯一运行输出目录（日志、校准、Drizzle、HiPS、legacy HISS validation artifacts）
- `AstroCS.wiki/` — 本地 Wiki 克隆
