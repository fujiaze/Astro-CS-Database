# 01 当前基线与已知缺口

本文件是 Agent 开始修改前的核查假设，必须以当前工作目录实际代码为准重新确认。

## 1. 已确认的主链

```text
READ_FITS → CALIBRATE → PLATESOLVE → PSF → PHOTOMETRIC → SNR → DRIZZLE
HISS × N → GRADIENT_SPHERE / STACK → HCSD
```

Orchestrator 当前使用 `astro_image_io` 内的 PipelineFrame 副本；独立 `data_pipeline` 模块仍处于拆分中间态。

## 2. 高优先级缺口

### G-001 重复星点检测

板解算内部调用 `sdet_detect_ex`，求解结束后 Orchestrator 又调用一次 `sdet_detect_ex` 生成 `star_det`，PSF 消费第二次结果。风险：重复耗时、结果顺序不一致、参数漂移、不同星集导致 WCS 与 PSF 不同源。

### G-002 PSF 输入量化

PipelineFrame 中图像为 float32，Dynamic PSF 仅接收 uint16，导致 PSF 阶段再次进行 clip 与转换。负值、高动态范围和小数信息会丢失。

### G-003 Gaia 数据重复且语义混杂

板解算会查询 astrometric Gaia catalog；Orchestrator 额外生成 `gaia_cat`；测光模块又查询 DR3SP 光谱。后两者不是完全相同的数据需求，不能简单共用三列 `ra/dec/mag`，但应消除无消费者的重复查询并建立缓存。

### G-004 校准未形成真实输入链

必须确认 Master Bias/Dark/Flat、坏点数据、温度和曝光匹配确实传入校准模块。空指针透传不得作为真实 Stage 1 成功。

### G-005 配置参数存在“记录但未生效”风险

`gaia-data`、`calibration-dir`、`filter`、QE、线程数、Drizzle 参数等必须建立命令→配置→模块调用的追踪测试。

### G-006 静默跳过

多个 stage 在 DLL 未加载、块为空时可能记录警告后返回 true。生产 CLI 必须严格失败；仅显式 `--allow-optional-*` 才能跳过非必需诊断。

### G-007 检查点只有控制状态

Stage 1 内存 Frame 未持久化时，不能从中间 stage 直接恢复。恢复边界必须是原始输入重跑、正式 `.aio` 快照，或已经完成的 HISS。

### G-008 Stage 2 源码可用性

上下文导出中可能缺少 `healpix_drizzle` / `healpix_stack` 源码。Agent 必须检查实际仓库、同级目录、历史、远端和现有构建产物；不能仅凭上下文包判定永久缺失。

### G-009 HISS/HCSD 科学追溯不足

必须明确格式版本、输入哈希、配置快照、算法版本、质量统计、滤光片和来源。HCSD 当前若只保存最终 pixel/ipix，后续需要决策覆盖数、方差、权重和拒绝计数是否进入正式通道。

## 3. 修改前强制基线

Agent 必须保存：

- 当前 commit、分支、工作树；
- 编译器、CMake/Make、Qt、第三方库版本；
- 所有 DLL 的 SHA-256 与导出符号；
- 固定真实帧的旧路径日志、耗时、WCS、星数、PSF 成功率、测光统计、HISS 元数据；
- 固定 HISS 集合的旧 Stage 2 输出；
- 旧路径中每帧 `sdet_detect_ex` 实际调用次数。

没有旧基线，不允许宣称新实现“无回归”。
