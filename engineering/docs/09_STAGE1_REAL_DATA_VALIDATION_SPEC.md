# 09 Stage 1 真实数据验收

## 1. 入口条件

- 所有必需源码和 DLL 可构建；
- PlateSolve 全量 TestData 路径决策已完成；
- 选定路径满足每帧单次检测并输出 `star_det v1`；
- 校准帧与 Gaia 数据登记完成；
- CLI 配置追踪测试通过；
- HISS writer/reader round-trip 通过。

## 2. 执行证据

每帧保存：命令/request、有效配置、模块清单、完整日志、事件流、阶段耗时、峰值内存、块摘要、输出 hash、PlateSolve detection path 和 inspect 结果。

## 3. 阶段断言

- READ：尺寸、Header、数据类型正确。
- CALIBRATE：实际 Master 被使用，空指针/透传禁止；无效值与坏点有统计。
- DETECTION/PLATESOLVE：
  - 路径 A：STAR_DETECT 一次，PLATESOLVE 只消费 `star_det`；
  - 路径 B：PLATESOLVE 保持原始内部检测路径并在同次调用导出 `star_det`；
  - 两种路径都要求 WCS/SIP 可正反投影，质量过门限，且总 detector 调用次数为 1。
- PSF：消费选定路径产生的同一 `star_det`，使用 float32 API，成功率和数值范围合理。
- PHOTOMETRIC：filter/QE/DR3SP 生效，匹配数与残差达标，scale 应用到 data。
- SNR：控制点数量、球面坐标、median 和 photometric scalar 有限。
- DRIZZLE：HISS 临时写入、原子提交、重新打开与元数据验证成功。

## 4. 负面测试

缺 Master、错误 Gaia 路径、不支持 filter、缺少选定路径 API、坏 FITS、WCS 失败、PSF 为空、测光匹配不足、磁盘写满/权限失败均必须非零退出并产生稳定错误码。

生产运行不得因选定 API 缺失而静默切换另一条 PlateSolve 数据路径。

## 5. 最终 Gate

所有 canonical 帧连续运行 3 次，无静默退化、无输出差异超限、无资源泄漏；才允许进入 Stage 2 系统验收。
