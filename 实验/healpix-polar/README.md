# 实验单元：healpix-polar（极区面积交叠）

## 这是什么

ACSD 的 `mosaic` 阶段把源帧 drizzle 到 HEALPix 网格时，需要精确计算"源像元 drop 足迹 × HEALPix 叶单元"的球面交叠面积。
本单元回答：**为什么该算法在极点破门、正确的极区算法是什么、各方案代价与适用域、以及"角点路径 ~1e-5 触底"的根因**。

- 报告：`docs/EXP-07-POLAR.md`
- 代码：`code/`（自包含 C++17 探针，固定 seed，不链接产品二进制）
- 结果：`results/`（扫描 CSV 与关键日志副本）
- 日志：`../../run/EXP-07-POLAR/logs/`（不入库）

## 一句话结论

极点破门有三条根因：叶边界"4 角 + 大圆弧弦"（RC1，单叶面积亏损最大 9.97e-2）、
`xyf2ang_replica` 的 `acos(z)` 角点构造（RC2a，只在 RC1 修好后可见）、
以及 TAN 逆投影在 `dec ≈ ±90°` 的 `asin` 量化（RC3，**不使闭合判据破门**，其影响是光度归一化偏差）。

**闭合判据对 RC1 近乎失明**：4 角弦多边形精确铺满球面，纯弦裁剪下闭合恒为 0（`use_fast=0` 实测 3.82e-14），
极点破门全部来自 `leaf_fully_inside_drop` 的假阳性（`full=376` 与破门数 376 一一对应）。RC1 的真判据是**逐叶误差**。

**破门不是极点专属**：`u+v = 1` 接缝（全天 12 条）实测 283/289 破门、最坏 1.802e-04，机制未定位。

最小改动面方案 = 稳定角点公式 + 叶边界自适应细分 + 覆盖自检（并把快路径与裁剪路径统一到同一面积口径），非极区零额外代价。

## 一键复现

```bash
bash run_all.sh quick    # 约 15 分钟：极点主消融 + 全天 14 角点扫描 + 负例 + 破门区间
bash run_all.sh full     # 约 60 分钟：追加 chart 原生对照、REC-1 预算扫描、计时、HST 两档
```

## 自查

```bash
python3 ../../run/EXP-07-POLAR/verify/check_report2.py  # 报告数字 vs 日志，59 项一致性自检
```

探针自身的正确性由 `code/p0_selftest.cpp` 把关：`rotate_to_z` 是否真把质心旋到 +z（12×9 方向，≤1.13e-16 rad），
以及 `vos_area_rotated` 是否与解析 drop 面积 `4·asin(h²/(1+h²))` 一致（一般位置 ≤3.4e-10）。

## 环境

- Linux amd64，`g++ -O2 -std=c++17`，单线程，无外部依赖（不链接任何产品二进制）。
- 全部运行带 `timeout` 并由 `/usr/bin/time -v` 记录峰值 RSS（实测 < 130 MB）。

## 目录约定

沿用仓库既有实验单元约定 `实验/<kebab-topic>/{code,docs,results,README.md}`。

## 与生产代码的关系

本单元**只读** `lib/algorithms/drizzle/healpix_drizzle/` 与 `lib/algorithms/shared/healpix/`（编译进探针做交叉核对），
不修改任何生产文件；修复方案以伪代码形式写在报告 §6.1，落地属于单独任务。
