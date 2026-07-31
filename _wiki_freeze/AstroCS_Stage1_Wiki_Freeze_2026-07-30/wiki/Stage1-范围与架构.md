> **SUPERSEDED — DO NOT IMPLEMENT**
>
> 本页面已被新标准页面取代，仅保留作历史迁移参考，不得作为实现依据。
> 请参阅 Home.md 中的"Stage1 标准页面（权威来源）"获取最新冻结规范。

# Stage1 范围与架构

## 状态

**已冻结**

## 1. 定位

Stage1 CLI 是面向高级用户和未来 GUI 的底层运算框架。

CLI 负责：

- 确定性执行单帧运算；
- 返回稳定结果、状态和硬错误；
- 生成 HISS；
- 提供机器可调用接口。

CLI 不负责：

- 用户交互；
- 警报和确认；
- 数据分组；
- 多夜或 session 管理；
- 设备归类；
- 校准帧自动匹配；
- 工作流编排；
- 软件发布界面。

这些由未来 GUI 完成。

## 2. 数据范围

AstroCS 1.0 Stage1 只接受：

- 单色 Light；
- 已制作完成的 Master Bias；
- 已制作完成且包含 Bias/Offset 的 Master Dark；
- 已校准并归一化的 Master Flat；
- GUI 或高级用户明确传入的参数。

不支持：

- CFA/Bayer 数据；
- Debayer；
- RGB/多通道输入；
- 原始 Bias/Dark/Flat 子帧；
- Master 制作；
- Overscan；
- 裁切；
- session/date；
- 自动分组与匹配；
- Flat Dark 工作流；
- 图像注册；
- 图像叠加。

主校准帧制作器未来以外挂工具形式提供，不纳入 Stage1。

## 3. 唯一主线

```text
READ
→ CALIBRATE
→ PLATESOLVE
→ PSF
→ PHOTOMETRIC
→ SNR
→ DRIZZLE
→ HISS_WRITE
```

任何阶段不得被“成功但跳过”替代。缺少必需输入或算法失败时返回硬错误。

## 4. PlateSolve 与 PSF

已冻结：

- PlateSolve 内部只检测一次星点；
- 同一份检测结果用于板解算；
- 同一份检测结果导出给 PSF；
- 不允许恢复第二次全图星点检测。

## 5. 当前开发边界

当前只开发 Stage1。

允许并行的唯一旁线：

- HISS 浏览器适配，因为它用于检查 Stage1 输出。

明确停止：

- Stage2；
- HCSD；
- 全局梯度；
- 多帧排异与叠加；
- 710 帧回归；
- 发布网站实现。

## 6. 发布前版本语义

发布前不维护 HISS v1/v2 等产品版本分支。

- 软件目标统一为 AstroCS 1.0；
- HISS 只有一份正在冻结的正式格式；
- 旧文件和 Python 原型只作为迁移来源；
- 文件内部允许保存 schema fingerprint、feature flags 和布局识别信息，以防错误读取，但不形成面向用户的 v1/v2 路线。
