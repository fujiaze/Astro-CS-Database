# Phase2 执行日志（2026-08-10 续）

控制包：`AstroCS_Phase2_Implementation_Control_Package_V1.zip`
（SHA256 `34A532A2451C8746BEF7B5DA05C3C4C7D15201D66A9D5F6AB5F8F291BE2EB308`）

## 前序（main 已有，gate 12/12）

W0 盘点 → W1 Wiki → W2 接口冻结 → W3 coverage 骨架 → W4 UPM CPU reference →
W5 dense 首版 → W6 block planner → W7 sigma/winsorized/avgsigma/ESD →
W8 weighted integration 库级 → W9 ACR legacy launcher → W10 robustness。

## 本会话完成（真实链闭合）

| 阶段 | 内容 | commit |
| --- | --- | --- |
| W3 | coverage 真实 AIO 实现（union MOC/target_order/兼容校验） | `63172ef` |
| W4 | control sampler + UPM 真实哈希/持久化/连通分量 | `d86fa9b` |
| W5 | dense cache 完整（source-hash checksum + stale 拒绝） | `627f4cf` |
| W7 | LinearFit + RCR 实现 | `018fbe8` |
| W8 | astrocs-stage2 正式入口（真实 block 校准/排异/叠加→AIO HiPS） | `305dd1b` |
| docs | 模块 memory.md/README | `0c249a9` |

## Gate 补测（2026-08-10 续）

| Gate | 内容 | commit |
| --- | --- | --- |
| G6 | Oracle 对照（Astropy sigma_clip 200/200、NIST/scipy ESD 120/120、SciPy winsorize）；修复偶数中位数 MAD bug 与 NIST t 分布 ESD 临界值 | `7060ba6` |
| G5 | 块尺寸不变性（4 档 chunk 1e-12 一致）+ 帧顺序不变性（frame_id 内容稳定 FNV-1a，参考帧=最小 hash） | `ef144a1` |
| G9 | stage2 阶段 profile（control_sample 9.2s、tiles_process 12.7s） | `ea1751f` |
| G11 | diagnostics.json（rejection 直方图） | `ea1751f` |
| G10 | WSL ASan+UBSan+LSan 0 错误 + 2000 随机 rejection/100 随机 UPM fuzz | `dff4998`/`fa77364` |
| G6 | 全量 synthetic matrix：616 组合（11 深度 × 8 污染 × 7 方法） | `658c02b` |
| G9 | 真实 CUDA mosaic_reject kernel（bridge 扩展 + CPU/GPU 等价 gate） | `f9a06ac` |
| G9 | stage2 逐 tile ACR 路由：真实 61.6M 像素 CPU/GPU max diff 7.45e-9，gate 22/22 | `f70080c` |
| G4 | AIO model APIs：aio_upm_* 容器（sparse/dense + checksum + stale），UPM I/O 迁移唯一 AIO；共享 crypto/sha256 | `fb52ba1` |

## 验证

- 合成 gate 22/22 PASS（含 CUDA CPU/GPU 等价 + stage2 路由）。
- Phase1 AIO 回归：pipeline_frame_contract 28/28、dataflow_fuzz 8668/8668。
- 真实 crop（T2/T3/t4_crop，互不重叠）：51 tiles、6.99M px 单覆盖 fallback。
- 真实重叠（t4_crop×t4_full）：285 tiles、61.59M px，重叠区 4.02M px
  2 样本 weighted integration，rejected=0（两帧同源），UPM 1 分量、
  frame offset = 0 / -0.001057。
- 最终完整三片（T2/T3/t4_full）：312 tiles、64.56M px，21.3s。
- Hipsgen：signal/support LINT（IVOA 1.0 compatible）+ CHECKCODE/CHECK +
  CHECKDATASUM（418 files）全过。

## 未完成

- ACR GPU kernel + stage2 逐 tile 路由已完成（sigma/winsorized 走 GPU，
  其他方法 CPU；真实输出 CPU/GPU max diff 7.45e-9）。
- AIO model APIs（aio_upm_*）已完成，UPM sparse/dense I/O 全部走唯一 AIO。
- Oracle 全量 synthetic matrix 已完成（616 组合）。
- Aladin GUI smoke（无 GUI 环境）。
- weight/rejection_count 作为 Image HiPS 产品（当前为 JSON 诊断）。
