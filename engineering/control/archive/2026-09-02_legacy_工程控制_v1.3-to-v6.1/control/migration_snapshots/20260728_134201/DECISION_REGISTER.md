# 决策登记

- ADR-v1.2-001：原 G0–G8 保留，新阶段使用 G9–G16。
- ADR-v1.2-002：PlateSolve 正式模式命名为 INTERNAL_DETECTION_SHARED_EXPORT（待 P09-002 核实提交）。
- ADR-v1.2-003：WCS 修复必须在生产端统一完成，具体符号转换由闭环测试决定。
- ADR-v1.2-004：浏览器先优化 v1 I/O/异步/GPU Tile；HCSD LOD 格式仅条件启动。

## P09-001 v1.1 过度结论修正（2026-07-27）

依据 `docs/01_V11_AUDIT_CORRECTION.md`，下列 v1.1 结论在本阶段不得继续引用为"已通过"：

- **CORR-001**：测光链未完成。P05-002 中 6/6 成功帧 `photometric_n_matched=0-1` 实为 `sigma_residual<=0 降级`的退化路径，不能等同于测光链有效。后续 P11-001 ~ P12-006 必须完成 Gaia→像素→PSF 空间匹配闭环。
- **CORR-002**：SNR² 加权仅在合成数据上证明。P06-002 T7 用合成 HISS (has_snr=1) 证明数学正确（输出=18.0=加权均值），但真实观测 HISS has_snr=0 退化为等权。后续 P12-005 必须修复 SNR 持久化。
- **CORR-003**：梯度校正未在真实非零梯度数据上验证。P06-002 T5/T6 用 C003 副本（字节级一致），fit_rms=0.0，仅证明管线可运行。后续 P14-004/005 必须用银心三片 Red 32 帧真实数据验证。
- **CORR-004**：三面板马赛克未在真实数据上验证。P06-003 Stage2 输入是 C003 副本，mean_pixel_count=1.9850（部分重叠，仅 2 帧），未验证真实大尺度三面板、非零梯度、真实 SNR² 加权。后续 P14-001 ~ P14-008 必须用银心三片 Red 32 帧真实数据验证。
- **CORR-005**：浏览器性能无 Gate。v1.1 P08-002 仅做 GUI 依赖架构分析，未做性能 Gate。架构已知问题：HCSD 打开会量读一次；按叶读取重复开文件/解头；渲染线程同步加载；CPU 每顶点查值并重传 VBO。后续 P15-001 ~ P16-006 必须做完整性能基线与异步 I/O + GPU Tile Renderer 改造。
- **CORR-006**：PlateSolve 共享检测导出保留。710 帧 A/B 无回归已验证，主线有效。命名不统一需 P09-002 修正为 `INTERNAL_DETECTION_SHARED_EXPORT`，不重写算法。

**基线锁定（不可篡改）**：

- v1.1 HEAD commit: `ed145a7 docs: 补充项目 README + 生成 v1.1 审计包`
- v1.1 分支: `main` (origin/main)
- v1.1 控制文件 SHA-256:
  - `engineering/control/PROJECT_STATE.yaml` = `55C6F9120B27BF83965B163D2F09E3502C49F269E06F9579EED3CF91395D0951`
  - `engineering/control/MASTER_TASK_REGISTER.csv` = `09E476835F216CE873FCCD0D904B40D43FB0DE81092AC8077B289137FC4AC6FD`
  - `engineering/control/CURRENT_TASK.md` = `CF87C7CD5AA60E06270C232581A3A94A13DF31633DD82ACBD5B0F657F878D7E8`
