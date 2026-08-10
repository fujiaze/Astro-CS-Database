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

## 验证

- 合成 gate 18/18 PASS。
- 真实 crop（T2/T3/t4_crop，互不重叠）：51 tiles、6.99M px 单覆盖 fallback。
- 真实重叠（t4_crop×t4_full）：285 tiles、61.59M px，重叠区 4.02M px
  2 样本 weighted integration，rejected=0（两帧同源），UPM 1 分量、
  frame offset = 0 / -0.001057。
- 最终完整三片（T2/T3/t4_full）：312 tiles、64.56M px，21.3s。
- Hipsgen：signal/support LINT（IVOA 1.0 compatible）+ CHECKCODE/CHECK +
  CHECKDATASUM（418 files）全过。

## 未完成

- ACR GPU kernel（仅 legacy CPU launcher）。
- Oracle 全量 synthetic matrix（Astropy/NIST/Siril/IRAF/RCR）。
- Aladin GUI smoke（无 GUI 环境）。
- weight/rejection_count 诊断产品（可选未输出）。
- 根目录 memory.md 同步 Phase2 状态。
