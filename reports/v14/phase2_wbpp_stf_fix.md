# Phase2 WBPP 排异 + STF 三控制点修复（V14 审核后，HEAD 59a3c6f）

## 1. 排异：WBPP 方法全集 + auto 选择（已进入生产路径）

`rejection.method` 现支持 11 种（rejection.h / stage2_common.cpp）：

```text
none / sigma / winsorized_sigma / averaged_sigma / linear_fit /
generalized_esd / rcr / percentile / median_sigma / minmax / auto
```

- 新增 percentile（Siril 相对 median 百分比 clip）、median_sigma
  （median+SD 迭代 clip）、minmax（每轮剔除 min/max）；
- winsorized_sigma 升级为 robust 版（median 位置 + 1.5σ winsorize
  迭代 ×1.134，对齐 Siril 1.4.3 rejection_float.c）；
- `auto` 按像素有效样本数选择（对齐 PixInsight WBPP Auto 语义）：

```text
n < 3   → none
3-5     → winsorized_sigma
6-10    → averaged_sigma
n > 10  → linear_fit
```

> 边界条件请用户对照 wbpp 文档核对，如有出入改一行即可。

## 2. 卫星线剔除能力（背景 0.002 + 卫星线 0.05 实测）

```text
n=2（GC/t4 overlap 现状）: 永不拒绝（z_max=0.67<3，统计死区）
n=5（auto→winsorized）:   拒绝 ✓
n=8（auto→averaged）:     拒绝 ✓
n=12（auto→linear_fit）:  拒绝 ✓
percentile/minmax:        各自有效 ✓
```

GC/t4 每 panel 单帧、overlap 仅 2 样本——卫星线需每 panel ≥3 帧
（或单帧 trail 修复，另行立项）才能真正剔除。

## 3. STF 面板：单条渐变 + 三控制点 + 比例尺缩放

- 4 滑块 + 曲线预设下拉 → 移除；单条渐变 + 暗部截止/中间调/亮部截止
  三控制点（点击条上任一点吸附最近控制点）；
- 手动控制点按显示空间裁剪窗口生效（compression 由当前曲线预设决定，
  修复 asinh c=0 → NaN 全黑根因）；
- 滚轮在条上放大/缩小比例尺（以鼠标为中心），双击恢复全览；
- 中间调范围 [0.001, 0.999]（可拉到两端）。

数值验证（browser_cli --stf-manual-probe，GC Wide）：

```text
midtones=0.05 → 81.1% 像素亮；midtones=0.95 → 0.97% 亮；PASS
```

## 4. 已知问题（如实标注）

1. **synthetic gate 复跑 2 个真实数据测试失败**：Phase2Sampler.
   RealHipsControlSampling / G6LocalSnrAvailabilityThreeZones，
   n_obs=0。根因：run/temp/phase1_freeze 的 T2_v3 与 T3_v3 为不同天区
   （leaf tile 零交集），且 T2 无 snr 产品——run/temp 数据漂移
   （非 git 管理），与本次改动无关（合成测试全过、rejection 9/9、
   oracle 51/51 PASS）。修复方向：改用已知 overlap 的
   t4_crop_v3 + t4_full_v3_final（待确认后执行）。
2. WBPP auto 的 n 区间边界待用户对照 wbpp 文档核对。
