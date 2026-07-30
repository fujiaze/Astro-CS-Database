# D-001 测试报告 — 三片 HISS 球面重合与光度一致性

- 测试编号: D-001
- 测试日期: 2026-07-30
- 测试类型: 球面重合 + 光度一致性验证
- 被测对象: output/D-001/T4_RED_GalaxyCenter_panel{1,2,3}.hiss (银心三片 Red HISS, nside=512 nested)
- 验证脚本: engineering_authoritative/evidence/D-001/verify_overlap.py
- 统计数据: engineering_authoritative/evidence/D-001/overlap_stats.json

## 1. 验收标准

| ID | 验收项 | 标准 | 依据 |
|---|---|---|---|
| A1 | 三片 HISS 全部生成 | 3 个 .hiss 文件存在且非空 | 任务要求 |
| A2 | 球面网格统一 | nside/nested 三片一致 | 球面可比性前提 |
| A3 | 真实球面信号重合 | 相邻片 ipix 交集 > 0 (非元数据比较) | 任务明确要求 |
| A4 | 三片都验证 | panel1/2/3 均参与(非只 panel1) | 任务明确要求 |
| A5 | 无镜像 | det(CD) 三片同号(手性一致) | 任务要求 |
| A6 | 光度尺度稳定 | 重叠区 signal 比值中位数 \|median-1\| < 0.15 | 任务要求 |

## 2. 测试结果

### A1 三片 HISS 生成 — PASS

| 文件 | 大小(B) | n_pix | exit code |
|---|---|---|---|
| T4_RED_GalaxyCenter_panel1.hiss | 87433 | 3928 | 0 |
| T4_RED_GalaxyCenter_panel2.hiss | 87461 | 3927 | 0 |
| T4_RED_GalaxyCenter_panel3.hiss | 87430 | 3936 | 0 |

### A2 球面网格统一 — PASS

- nside: {512} (三片一致)
- nested: {True} (三片一致)

### A3 真实球面信号重合 — PASS

通过 hiss_v2.v1_read_snr_model() 读取 V1 .hiss 的 ipix/signal 数组, 计算 HEALPix ipix 集合交集:

| 重叠对 | 交集 ipix | 占比 |
|---|---|---|
| panel1 ∩ panel2 | 792 | 20.16% / 20.17% |
| panel2 ∩ panel3 | 879 | 22.38% / 22.33% |
| panel1 ∩ panel3 | 0 | 不相邻(panel2 居中) |

相邻片共有 1671 个 HEALPix 像素真实球面重合。panel1-panel3 无交集是马赛克线性南北排列的正常几何结果, 非异常。

### A4 三片都验证 — PASS

panel1/panel2/panel3 全部参与 ipix 交集计算和 signal 比较, 非仅 panel1。

### A5 无镜像 — PASS

| panel | det(CD) | det 符号 |
|---|---|---|
| panel1 | 3.070e-06 | +1 |
| panel2 | 3.071e-06 | +1 |
| panel3 | 3.069e-06 | +1 |

det(CD) 三片同号(正), 手性一致, 无镜像。

注: panel2/panel3 的 CD[0,0]/CD[1,1] 符号与 panel1 相反, 为 180° 相机旋转(刚体变换, det 不变号), 非镜像。pixscale 相对差异 0.033% (6.307"-6.309"), 同设备高度一致。

### A6 光度尺度稳定 — PASS

| 重叠对 | n_valid | ratio median | \|median-1\| | scatter(p84-p16) | 判定 |
|---|---|---|---|---|---|
| panel1_vs_panel2 | 792 | 0.9814 | 0.0186 | 0.1645 | PASS |
| panel2_vs_panel3 | 879 | 0.9878 | 0.0122 | 0.0766 | PASS |

两对重叠区比值中位数均接近 1.0 (偏差 < 2%), 光度尺度稳定。

## 3. 结论

| 验收项 | 结果 |
|---|---|
| A1 三片 HISS 生成 | PASS |
| A2 球面网格统一 | PASS |
| A3 真实球面信号重合 | PASS |
| A4 三片都验证 | PASS |
| A5 无镜像 | PASS |
| A6 光度尺度稳定 | PASS |
| **总体** | **PASS (6/6)** |

所有验收项通过。三片银心 Red HISS 在球面上正确重合, 光度尺度一致, 无镜像。panel2/panel3 相对 panel1 存在 180° 相机旋转差异(非镜像), Drizzle 球面投影已正确处理。

## 4. 证据索引

- overlap_stats.json — 完整统计数据(WCS/重叠/比值/镜像检查)
- overlap_analysis.png — 4 子图可视化(球面位置/signal散点/比值直方图/覆盖范围)
- verify_overlap.py — 可复现验证脚本
- configs/stage1_config_T4_Red_panel{2,3}.json — Stage1 配置
- logs/stage1_panel{2,3}.log — Stage1 运行日志
