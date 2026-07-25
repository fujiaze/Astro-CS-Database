# 07 配置、路径与依赖规范

## 1. 配置分层

- built-in defaults：只包含安全、可移植默认值；
- project config：项目级 Gaia、校准库、输出、设备和算法默认；
- request config：单任务输入和覆盖；
- CLI override：测试和高级调用。

每次任务必须输出 `effective_config.json` 和 SHA-256。

## 2. 禁止隐式路径

算法不得根据当前工作目录猜测 Gaia、曲线、DLL 或校准帧。所有路径由 Orchestrator 解析为规范绝对路径后传入模块。

## 3. 必需参数追踪

必须建立测试证明以下参数真正到达消费者：

- Gaia astrometry 与 DR3SP 数据目录；
- Master Bias/Dark/Flat 与坏点数据；
- filter、QE、光谱波长；
- focal length、pixel size、RA/Dec 先验；
- nside、ordering、pixfrac；
- sigma clip、Winsor、SNR 权重；
- 模块线程数、内存预算和超时。

## 4. 能力协商

CLI 启动时查询模块版本、ABI 和可用导出。重复检测迁移后，生产链要求：

- plate_solve 必须提供由全量 TestData 决策选定的 API：路径 A 为 `ipv_solve_from_detections_v1`，路径 B 为保持原求解流的 detection-sink API；
- dynamic_psf 提供 `dpsf_fit_batch_f32`；
- PipelineFrame 支持所需 schema；
- HISS/HCSD I/O 支持目标格式版本。

能力不满足必须在处理输入前失败。

## 5. 依赖锁定

记录编译器、运行库、第三方库、DLL hash 和源 commit。构建产物不能只用文件名判断版本。
