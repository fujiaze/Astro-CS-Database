# IPV Plate Solver 真实数据测试报告

**测试日期**: 2026-07-02
**测试模块**: IPV (Iterative Polygon Voting) plate solving
**测试目标**: 用 20 帧真实天文图像验证 IPV 模块在不同 FOV/滤镜/曝光下的表现

---

## 一、测试环境

### 1.1 硬件与软件
- **CPU**: 16 线程
- **内存**: 64 GB
- **OS**: Windows
- **Python**: 3.12.2 (MSC v.1937 64 bit)
- **关键模块**: numpy 1.26.4, astropy 7.1.0, ctypes
- **编译器**: g++ 16.1.0 (MSYS2 MinGW64)

### 1.2 依赖项检查 (30/30 全部通过)

| 检查项 | 状态 | 详情 |
|--------|------|------|
| Python 版本 | ✓ | 3.12.2 |
| numpy / astropy / ctypes | ✓ | 1.26.4 / 7.1.0 / OK |
| testdata 目录 | ✓ | 含 NGC6302, NGC7293, M20, NGC247, NGC55, LDN43, Galaxy_Center |
| GaiaDR3SP 数据库 | ✓ | 20 个 .xpsd 文件 (2.2 亿星) |
| GaiaDR3 数据库 | ✓ | 16 个 .xpsd 文件 (18 亿星, 备用) |
| ipv_solver.dll | ✓ | lib/plate_solve/cpp/ipv/ipv_solver.dll |
| gaia_client.dll | ✓ | lib/gaia_xpsd_client/gaia_client.dll |
| star_detector.dll | ✓ | lib/star_detector/star_detector.dll |
| astro_image_io.dll | ✓ | lib/astro_image_io/astro_image_io.dll |
| libgomp-1.dll (OpenMP) | ✓ | C:\msys64\mingw64\bin\libgomp-1.dll |
| GaiaClient 句柄 | ✓ | db_type=2 (DR3SP), 银心 0.5° 查询返回 15576 颗星 |
| StarDetector 句柄 | ✓ | fitRadius=0 |
| IPVSolver 实例化 | ✓ | polygon_sides=6, n_pivot=30 |

### 1.3 数据库选择
本测试优先使用 **GaiaDR3SP** (db_type=2, 2.2 亿星, 光谱版)。
回退顺序: GaiaDR3SP → GaiaDR3。

---

## 二、测试帧列表 (20 帧)

按 FOV 覆盖窄/中/宽三类:

| FOV 类别 | 目标 | 焦距 | 像素尺寸 | 图像尺寸 | FOV | 帧数 |
|----------|------|------|----------|----------|-----|------|
| 窄 (<1°) | NGC6302 | 6800mm | 9.0um | 4096×4096 | 0.311° | 3 |
| 中 (1-3°) | NGC7293 | 1917mm | 9.0um | 4096×4096 | 1.102° | 3 |
| 中 (1-3°) | M20_T2 | 1917mm | 9.0um | 4096×4096 | 1.101° | 3 |
| 中 (1-3°) | NGC247_T2 | 1917mm | 9.0um | 4096×4096 | 1.102° | 3 |
| 中 (1-3°) | NGC55_T3 | 1877mm | 9.0um | 4096×4096 | 1.125° | 3 |
| 中 (1-3°) | LDN43 | 1917mm | 9.0um | 4096×4096 | 1.101° | 3 |
| 宽 (>3°) | Galaxy_Center_mosaic1 | 200mm | 6.0um | 4500×3600 | 7.735° | 2 |

**注**: 任务描述中将 LDN43 归为宽 FOV, 但实际其焦距为 1917mm (FOV≈1.1°), 应属中 FOV。
真正的宽 FOV 只有 Galaxy_Center_mosaic1 (200mm 短焦, FOV≈7.7°)。

---

## 三、测试结果汇总

### 3.1 总体结果

| 指标 | 数值 |
|------|------|
| 总帧数 | 20 |
| 求解成功 (success=True) | 10/20 (50.0%) |
| WCS 验证通过 (astropy) | 10/20 (50.0%) |
| 中位耗时 | 0.88s |
| 平均耗时 | 1.37s |
| **目标成功率** | **>90%** |
| **实际成功率** | **50%** ✗ |

### 3.2 按 FOV 分类

| FOV 类别 | 成功/总帧 | 成功率 |
|----------|-----------|--------|
| 窄 (<1°) | 3/3 | **100%** ✓ |
| 中 (1-3°) | 7/15 | 46.7% |
| 宽 (>3°) | 0/2 | 0% |

### 3.3 按目标分类

| 目标 | FOV 类 | 成功/总帧 | 成功率 |
|------|--------|-----------|--------|
| NGC6302 | 窄 | 3/3 | 100% ✓ |
| NGC7293 | 中 | 3/3 | 100% ✓ |
| NGC247_T2 | 中 | 3/3 | 100% ✓ |
| LDN43 | 中 | 1/3 | 33% |
| M20_T2 | 中 | 0/3 | 0% ✗ |
| NGC55_T3 | 中 | 0/3 | 0% ✗ |
| Galaxy_Center | 宽 | 0/2 | 0% ✗ |

---

## 四、详细结果表

| 标签 | FOV类 | FOV° | 状态 | RMS_px | 匹配对 | 内点 | 模式 | 耗时(s) | 中心偏差(") | 尺度误差(%) |
|------|-------|------|------|--------|--------|------|------|---------|-------------|-------------|
| NGC6302_01 | narrow | 0.311 | pass | 2601.4 | 39 | 39 | 0 | 0.67 | 0.39 | 0.18 |
| NGC6302_02 | narrow | 0.311 | pass | 2721.5 | 39 | 39 | 0 | 0.74 | 0.39 | 0.16 |
| NGC6302_03 | narrow | 0.311 | pass | 2678.1 | 15 | 15 | 0 | 0.78 | 0.39 | 0.19 |
| NGC7293_01 | medium | 1.102 | pass | 2003.4 | 39 | 39 | 0 | 0.87 | 1.38 | 0.45 |
| NGC7293_02 | medium | 1.102 | pass | 2047.9 | 43 | 43 | 0 | 0.88 | 1.38 | 0.47 |
| NGC7293_03 | medium | 1.101 | pass | 1943.4 | 39 | 39 | 0 | 0.84 | 1.38 | 0.45 |
| M20_T2_01 | medium | 1.101 | exception | 0.0 | 0 | 0 | -1 | 2.07 | - | - |
| M20_T2_02 | medium | 1.101 | exception | 0.0 | 0 | 0 | -1 | 1.97 | - | - |
| M20_T2_03 | medium | 1.101 | exception | 0.0 | 0 | 0 | -1 | 2.02 | - | - |
| NGC247_T2_01 | medium | 1.102 | pass | 2037.3 | 50 | 50 | 0 | 0.83 | 1.37 | 0.41 |
| NGC247_T2_02 | medium | 1.102 | pass | 2027.8 | 36 | 36 | 0 | 0.87 | 1.38 | 0.44 |
| NGC247_T2_03 | medium | 1.101 | pass | 2080.4 | 61 | 61 | 0 | 0.93 | 1.37 | 0.41 |
| NGC55_T3_01 | medium | 1.125 | exception | 0.0 | 0 | 0 | -1 | 0.86 | - | - |
| NGC55_T3_02 | medium | 1.125 | exception | 0.0 | 0 | 0 | -1 | 0.82 | - | - |
| NGC55_T3_03 | medium | 1.125 | exception | 0.0 | 0 | 0 | -1 | 0.87 | - | - |
| LDN43_01 | medium | 1.101 | pass | 2080.3 | 105 | 105 | 0 | 1.88 | 1.37 | 0.41 |
| LDN43_02 | medium | 1.101 | exception | 0.0 | 0 | 0 | -1 | 1.45 | - | - |
| LDN43_03 | medium | 1.101 | exception | 0.0 | 0 | 0 | -1 | 1.41 | - | - |
| Galaxy_Center_01 | wide | 7.735 | exception | 0.0 | 0 | 0 | -1 | 3.83 | - | - |
| Galaxy_Center_02 | wide | 7.735 | exception | 0.0 | 0 | 0 | -1 | 2.74 | - | - |

**注**: `RMS_px` 字段为 C 端 `build_wcs` 阶段返回的 RMS, 数值偏大 (2000+ px) 疑似 C 端单位换算 bug (角秒/像素混用)。WCS 本身经 astropy 独立验证, 中心偏差 0.39"-1.38" 与尺度误差 0.16%-0.47% 均极佳, 表明 WCS 是正确的。

---

## 五、成功帧分析 (10 帧)

### 5.1 WCS 精度
- **中心偏差**: 0.39" - 1.38" (亚角秒级, 远低于 600" 阈值)
  - 窄 FOV (NGC6302): 0.39" (极佳, 长焦距 + 小像素尺度)
  - 中 FOV (NGC7293/NGC247/LDN43): 1.37" - 1.38" (优秀)
- **尺度相对误差**: 0.16% - 0.47% (亚百分级, 远低于 20% 阈值)
- **匹配对数**: 15 - 105 对 (PROSAC 内点)
- **耗时**: 0.67s - 1.88s (单帧 plate solving)

### 5.2 算法表现
- 所有成功帧的 `best_mode=0` (NONE, 即无翻转), 说明这些帧的图像方向与天球坐标系一致
- NGC6302 (窄 FOV) 内点数稳定在 15-39, LDN43_01 内点数高达 105 (星点密集)
- 耗时与检测星数呈正相关: NGC6302 (~700 星, 0.7s) → LDN43_01 (星点多, 1.88s)

---

## 六、失败帧诊断 (10 帧)

### 6.1 共同特征
所有 10 个失败帧的 C 端日志显示相同模式:

```
[ERROR] 所有 flip_mode 均失败, 终止求解
[ERROR]   诊断: detected_stars=XXX, catalog_stars=YYY, failed_modes=4/4
[ERROR]     mode 0: max_vote=0, candidates=ZZZ, n_inliers=2
[ERROR]     mode 1: max_vote=0, candidates=ZZZ, n_inliers=2
[ERROR]     mode 2: max_vote=0, candidates=ZZZ, n_inliers=2
[ERROR]     mode 3: max_vote=0, candidates=ZZZ, n_inliers=2
```

关键指标:
- `max_vote=0` (所有 4 个 flip_mode): 多边形投票阶段未产生任何有效投票
- `n_inliers=2` (最低限度): PROSAC 仅靠 2 点解析求解, 无法形成可靠匹配
- `detected_stars` 与 `catalog_stars` 数量正常 (50-374 / 71-571): 星点检测与 Gaia 查询均正常

### 6.2 根因分析
**问题出在 Phase 2 (geometric_vote 多边形投票阶段)**:
- 六边形描述符匹配未产生共识, 投票数为 0
- 可能原因:
  1. `sigma_d_arcsec` 阈值过严, 真实匹配对被滤除
  2. 多边形完整性验证逻辑在某些坐标配置下失效
  3. 初始指向 (OBJCTRA/OBJCTDEC) 与实际偏差超出算法容忍范围
  4. 翻转模式枚举不全 (4 种 mode 不足以覆盖所有相机方向)

### 6.3 与 V4.5 对比
参考 memory.md 中 V4.5 相对向量法的成功率:
| 目标 | V4.5 成功率 | IPV 成功率 | 对比 |
|------|-------------|------------|------|
| NGC6302 | 100% | 100% | 一致 ✓ |
| NGC7293 | 100% | 100% | 一致 ✓ |
| NGC55 | 98.7% | 0% | IPV 退步 ✗ |
| NGC247 | 89.7% | 100% | IPV 改进 ✓ |
| LDN43 | 40.5% | 33% | 一致 (均偏低) |
| M20_T2 | 0% | 0% | 一致 (均失败) |
| Galaxy_Center | 0% | 0% | 一致 (宽场均失败) |

**关键发现**:
- IPV 在 NGC247 上比 V4.5 改进 (89.7% → 100%)
- IPV 在 NGC55 上明显退步 (98.7% → 0%), 需调查
- M20_T2 / Galaxy_Center 两方法均失败, 属算法已知限制

---

## 七、结论与建议

### 7.1 结论
1. **环境完整**: 所有依赖项就绪 (30/30 检查通过), 测试可正常运行
2. **窄 FOV 完美**: NGC6302 (fl=6800mm, FOV≈0.31°) 3/3 全部成功, WCS 精度亚角秒级
3. **中 FOV 部分成功**: NGC7293/NGC247 100% 成功, 但 M20_T2/NGC55_T3 0% 失败
4. **宽 FOV 失败**: Galaxy_Center (fl=200mm, FOV≈7.7°) 0/2 失败
5. **总体成功率 50%**: 低于 90% 目标, 主要受 M20_T2/NGC55_T3/Galaxy_Center 拖累
6. **成功帧精度极佳**: 中心偏差 0.39"-1.38", 尺度误差 0.16%-0.47%, 内点 15-105 对

### 7.2 改进建议
1. **调查 max_vote=0 根因**: 重点检查 `geometric_vote` 在 M20_T2/NGC55_T3 上的行为
   - 检查 `sigma_d_arcsec` 自适应公式是否在这些帧上产生过小阈值
   - 检查六边形描述符的顶点排序是否在特定坐标变换下失效
2. **增加 flip_mode 数量**: 当前 4 种 (NONE/FLIP_X/FLIP_Y/FLIP_XY), 可考虑增加 90°/270° 旋转
3. **引入回退机制**: 当多边形投票失败时, 回退到 V4.5 相对向量法 (已知在 NGC55 上 98.7% 成功)
4. **宽场球面投影**: Galaxy_Center 失败可能源于大 FOV 下的球面投影畸变, 需用球面三角而非平面近似
5. **修复 C 端 RMS_px 计算**: 当前 `rms_px` 数值异常 (2000+), 疑似角秒/像素单位混用, 不影响 WCS 正确性但影响诊断

### 7.3 测试产物
- 测试脚本: `lib/plate_solve/python/test_real.py`
- 环境检查: `lib/plate_solve/python/test_real_env_check.py`
- JSON 结果: `lib/plate_solve/logs/ipv_real_test/test_real_results.json`
- CSV 汇总: `lib/plate_solve/logs/ipv_real_test/test_real_summary.csv`
- 运行日志: `lib/plate_solve/python/test_real_run.log`

---

## 八、运行测试方法

### 8.1 环境检查
```powershell
# 全局强制UTF-8编码
[Console]::InputEncoding = [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$env:Path = "$env:Path;C:\msys64\mingw64\bin"
cd "f:\Astro dev\Astro CS Normalization Database\lib\plate_solve\python"
py test_real_env_check.py
```
退出码 0 = 环境就绪, 1 = 有缺失项。

### 8.2 完整测试
```powershell
py test_real.py
# 或不写 C 端日志 (加速):
py test_real.py --no-log
# 指定 Gaia 数据库:
py test_real.py --gaia-dir "F:\Astro dev\Astro CS Normalization Database\GaiaDR3" --db-type 1
```

### 8.3 运行环境要求
- Windows + Python 3.10+
- MinGW64 (C:\msys64\mingw64\bin) 在 PATH 中 (libgomp-1.dll 用于 OpenMP)
- numpy, astropy (WCS 验证用)
- 项目依赖 DLL 全部就绪 (见 1.2 节)
- GaiaDR3SP 或 GaiaDR3 数据库可用
