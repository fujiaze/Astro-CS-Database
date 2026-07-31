# Stage1 范围与流水线

> 本页面为已冻结规范，源自 `02_FROZEN_STAGE1_HISS_SPEC.md`。Agent 可实现和文档化，不得自行改写科学语义。

## 1. Stage1 固定流水线

```text
单色 Light
→ 选择一种校准模式
→ PlateSolve
→ 复用同一批星点
→ PSF
→ Gaia 光谱积分 / 测光校准
→ 稀疏 SNR 控制点
→ 显式或自动 NSIDE
→ 高精度 HEALPix Drizzle
→ HISS
```

## 2. CLI 与 GUI 边界

CLI 只负责明确输入和参数下的确定性计算。以下属于 GUI 或外部工具，**不进入 Stage1 CLI**：

- 文件分组、Session、日期、设备、滤镜管理；
- Master 自动匹配和 Master 制作；
- CFA/Bayer/Debayer、多通道；
- overscan、裁剪、cosmetic workflow；
- 软警告和交互确认；
- Stage2 调度。

## 3. CLI 硬合法性检查

CLI 仍必须检查：

- 文件可读、允许格式、单色；
- 尺寸/通道匹配；
- 必需 Master 存在；
- NSIDE 合法；
- PlateSolve 成功；
- 输出可写。

## 4. 单色输入

Stage1 CLI 只接受单色 Light 帧。多通道图像由 GUI 或外部工具拆分为单色后再调用 CLI。

## 5. 星点复用

PlateSolve 内部只做一次全图星点检测。同一检测结果同时用于：

- 天文解算；
- PSF；
- 后续可复用的星点相关计算。

**不得重复执行第二次全图检测。**

## 6. 当前不讨论 Stage2

不得让现有 Stage2 反向限制 Stage1/HISS。Stage2 后续按最终 Stage1 标准修改。
