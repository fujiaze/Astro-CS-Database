# 02 CLI 内核架构规范

## 1. 组件职责

### CLI Shell

负责参数解析、机器协议、退出码、信号处理和进程级生命周期。不得实现算法。

### Orchestrator

是唯一编排中心，负责：

- 建立 JobContext 与 PipelineFrame；
- 解析合并后的有效配置；
- 加载并验证模块能力；
- 顺序调用 stage；
- 验证输入/输出数据块；
- 产生进度和质量事件；
- 管理取消、超时、失败、清理与持久化；
- 写出最终结果清单。

### PipelineFrame

只承载当前单帧运行时大数据和中间结果。单帧结束后销毁，不跨任务作为数据库使用。

### 算法模块

每个模块只消费契约中声明的数据，产生声明的输出。不得为了弥补上游缺块而偷偷重新执行上游算法。

### HISS / HCSD I/O

是 Stage 1/Stage 2 的正式持久化边界。写入必须采用临时文件 + flush/fsync + 原子替换或等价安全策略，防止半成品被当作成功输出。

## 2. 推荐 Stage 1 节点

为消除共享数据重复生产，Stage 1 调整为：

```text
READ_FITS
CALIBRATE
STAR_DETECT
PLATESOLVE
PSF
PHOTOMETRIC
SNR
DRIZZLE
```

`STAR_DETECT` 是显式内部节点，也应在 CLI 进度事件中可见。它不是 GUI 算法面板要求，而是稳定数据依赖所需。

## 3. Stage 2 节点

```text
LOAD_AND_VALIDATE_HISS
BUILD_OVERLAP_GRAPH
SOLVE_SPHERICAL_GRADIENT
ROBUST_STACK
WRITE_HCSD
```

底层可以由一个 DLL 实现，但 Orchestrator 的进度事件必须反映真实阶段，不能用空节点伪装。

## 4. 依赖方向

```text
CLI → Orchestrator → contracts/data_pipeline
                 ├→ astro_image_io
                 ├→ calibration
                 ├→ star_detector
                 ├→ plate_solve
                 ├→ dynamic_psf
                 ├→ photometric_calib
                 ├→ snr_estimator
                 ├→ healpix_drizzle
                 └→ healpix_stack
```

算法模块不得反向依赖 Orchestrator。格式与数据契约应放在稳定公共头文件或独立 contract 模块。

## 5. 线程与取消

- Orchestrator 持有取消 token；每个长阶段必须定期检查。
- 模块线程数由统一配置下发，禁止各模块各自读取环境变量后争抢全部核心。
- GUI 未来通过 CLI 发送取消请求，CLI 转换为 token；不能通过强制杀进程作为正常取消机制。

## 6. 严格成功语义

Stage 成功必须同时满足：

- 模块存在且能力版本满足；
- 输入数据块 schema 正确；
- 算法返回成功；
- 输出数据块存在且数值合法；
- 质量门限满足或明确标记为警告；
- 输出文件通过重新打开与完整性检查。

只打印“完成”或返回码 0 不足以判定成功。
