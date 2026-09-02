# P11-004 遗留问题记录（待 bundle 打包审核）

**状态**：DEFERRED（用户决定跳过，后续和 bundle 一起打包 zip 提交审核）
**日期**：2026-07-28
**决策来源**：用户明确指示"你把问题记录下来，跳过这个执行下一项任务，记录的问题后续和bundle一起打包zip，提交审核"

## 问题摘要

诊断工具 `wcs_closure_diagnostic.py` 重新 kd-tree 匹配产生的残差与 IPV 内部 RMS 不可比，gate 失败但 IPV 求解精度已验证正常。

## 已排除的根因（通过实验验证）

| 假设 | 验证方法 | 结果 | 证据 |
|------|---------|------|------|
| WCS 构建方式差异 | `compare_wcs_construction.py` 对比 `to_astropy_wcs(result)` vs `WCS(header)` | 8/8 帧 EQUIVALENT，投影差异 1e-10 px | `reports/wcs_construction_comparison.json` |
| WCS 自身闭环错误 | pixel→sky→pixel + sky→pixel→sky 双向闭环 | 误差 1e-10 px，WCS 内部一致 | `reports/T2_RED_LDN43_siril_v2/closure_report.json` |
| Gaia 查询路径差异 | 对比 `cone_search_gaia`（直接 ctypes）vs `gaia_client.cone_search`（Python 封装） | 同一 C API `gaia_client_cone_search_for_solver`，参数一致，只 mag dtype 不同（不影响坐标） | `lib/plate_solve/archive/vector_method/python/python/vector_match_v2.py:114` vs `wcs_closure_diagnostic.py:159` |
| 检测星点来源差异 | `solve_from_memory_with_callback` 注释确认"算法与 solve_from_memory 完全一致, 区别仅在 sdet_detect_ex 后调用 callback" | 检测星点与 IPV 内部一致 | `lib/plate_solve/python/ipv_solver.py:470` |
| CRPIX 0.5px 偏移 | 用户明确否定："我确定没有系统性偏移，解析精度极高" | 假设被用户否决 | - |

## 残留问题

### 现象

- T2_RED_LDN43 帧：
  - IPV 内部：34 对 inliers，RMS=0.12px（RANSAC 选最佳一致子集）
  - 诊断工具（siril_bright_first，最亮 60 颗 Gaia）：24 对，p68=0.82px，X mean=+0.60，Y mean=+0.53
  - 诊断工具（全星等 kd-tree，max_dist=3px）：1942 对，p68=1.02px，X mean=+0.53，Y mean=+0.80
- 8/16 帧 gate 失败（详见 P11-003 报告）

### 核心矛盾

诊断工具用 **kd-tree 双向最近邻匹配** 产生大量匹配对（含暗星误配 + 饱和星质心偏差），而 IPV 内部用 **RANSAC + Umeyama 迭代精化** 选 34 个一致 inliers。两者匹配策略不同，残差不可直接比较。

### 按星等分 bin 残差（全星等匹配，1942 对）

| Gaia mag | n | median | x_mean | y_mean |
|----------|---|--------|--------|--------|
| 6-7 | 1 | 1.684px | +1.684 | -0.004 |
| 8-9 | 4 | 0.696px | +0.529 | +0.466 |
| 9-10 | 3 | 0.752px | +0.641 | +0.634 |
| 10-11 | 16 | 0.760px | +0.549 | +0.563 |

**异常**：最亮星（mag 6-7）残差最大（1.68px），与"WCS 准则亮星质心应最准"的预期相反。该星 Y 残差≈0，X 残差=+1.68px，疑似饱和星质心 X 方向偏移或邻近星误配。

### 未验证的假设（待审核时决策）

1. **诊断工具应直接用 IPV result 的 RMS/n_pairs 作为 gate 指标**（IPV 已做 RANSAC 匹配），astropy WCS 等价性已由对比报告证明。不再重新 kd-tree 匹配。符合 siril 验证逻辑（RMS 作为精度指标）。
2. **siril_outlier_rejection 的 NSIGMA=10 过于宽松**（阈值 7.2px），未能剔除 1.68px 的饱和星异常。可调至 NSIGMA=3。
3. **检测星点质心坐标系 vs astropy world_to_pixel 像素约定**可能存在 0.5px 系统性偏差（X mean 持续 +0.53），但用户已否决此方向，且 visualize_reproject 人眼验证显示对齐。

## 备选修复方案（待审核决策）

| 方案 | 描述 | 优点 | 缺点 |
|------|------|------|------|
| A. 用 IPV RMS 作 gate | 诊断工具直接报告 IPV result 的 RMS/n_pairs/inliers 作为 gate 指标，不重新匹配 | 符合 siril 验证逻辑，避免匹配策略差异 | 失去"独立重新匹配"验证意义 |
| B. 严格匹配+剔除 | max_match_dist=1.0px + siril NSIGMA 从 10 降到 3 | 保留重新匹配，剔除异常 | 仍可能含暗星误配 |
| C. 视觉验证对齐 | 在 visualize_reproject 上同时画 Gaia 投影 + 检测星点，放大局部 200x200 区域 | 最直接的人眼验证 | 无法量化通过 gate |
| D. 查质心坐标系 | 检查 star_detector 的质心输出约定（0-based 像素中心 vs 像素左下角） | 根因定位 | 用户已否决 CRPIX 方向 |

## 当前证据产物

- `scripts/wcs_closure_diagnostic.py`（Siril v2 升级版，已实现自适应星等 + 亮星优先 + 迭代剔除）
- `scripts/compare_wcs_construction.py`（WCS 构建等价性验证，8/8 帧 EQUIVALENT）
- `scripts/verify_crpix_offset.py`（已创建未运行，用户否决此方向）
- `scripts/run_diagnosis.ps1`（批量诊断脚本）
- `reports/T2_RED_LDN43_siril_v2/`（Siril v2 诊断结果，gate FAIL）
- `reports/T2_RED_LDN43_nobright/`（全星等匹配诊断结果，gate FAIL）
- `reports/wcs_construction_comparison.json`（WCS 等价性证明）
- `reports/diagnosis_summary.json`（8 帧批量诊断汇总）

## 建议下一步

1. 将本 ISSUES_DEFERRED.md 和上述证据产物打包进 P11-004 bundle
2. 提交审核，由审核方决策采用哪个修复方案
3. 审核决策后，回到 P11-004 实施修复并重跑 16 帧验证
