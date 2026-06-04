# Star Detector 模块记忆

## 模块概述
星点检测C++模块，编译为star_detector.dll，Python ctypes绑定。

## 文件结构
- `include/star_detector.h` - 公共C头文件
- `src/sdet_api.cpp` - API实现（Moffat4拟合 + 检测流水线）
- `src/sdet_detector.cpp/h` - 检测引擎（结构图、连通域、去重）
- `src/sdet_image.cpp/h` - 图像处理工具（小波、滤波、插值）
- `src/sdet_log.cpp/h` - 日志系统
- `src/sdet_background.cpp/h` - SExtractor风格背景估计（新增）
- `python/star_detector.py` - Python ctypes封装
- `python/test_detection.py` - 测试脚本（直方图拉伸+标注输出）

## 状态历史

### 2026-05-26 迭代
- **新增背景估计**: 实现SExtractor风格网格化背景估计，参考源码back.c
  - 网格划分 → sex_backstat(2σ裁剪统计) → sex_backhisto量化 → sex_backguess迭代mode估计 → sex_filterback中值平滑
  - 双线性插值获取任意像素位置背景值/噪声值
- **修复Moffat4坐标**: dx=x-cx, img_cx = x0 + cx（之前错误用了窗口中心）
- **maxStars可选**: 默认0=不截断，>0时选择性限制候选=maxStars*2、拟合=maxStars
- **截断逻辑**: 候选/去重后/拟合后三处统一改为条件判断
- **Python默认参数**: maxStars默认值从10000改为0
- **AI签名清理**: 无AI生成签名，补充核心函数注释
- **示例图像**: README展示检测效果图片(test_output/example.jpg)
- **推送到GitHub**: 仓库已迁移到 https://github.com/fujiaze/Star-Detector-Cpp

### 测试结果
| 配置 | 候选 | 去重后 | 拟合成功 | 最终通过 | 耗时 |
|------|------|--------|---------|---------|------|
| 无截断(maxStars=0) | 30500 | 29100 | 15692 | 13936 | 16.9s |
| maxStars=10000 | 20000 | 10000 | 5548 | 5038 | 8.7s |
| maxStars=5000(旧) | 10000 | 5000 | 3381 | 3148 | 6.2s |

### 待解决
- 拟合成功率约54%(暗星拟合困难)，可考虑降低sx/sy下限或改进初始值估计
- sdet_background.cpp中sex_backhisto有未使用变量h(编译警告)

### 2026-05-27 饱和星点检测
- **新增API**: `sdet_detect_ex()` 输出坐标+flux+饱和标记, `sdet_free_detect_ex()`
- **饱和星点检测流程**:
  1. 半动态范围二值化: threshold = img_min + (img_max - img_min) * 0.5
  2. 连通域分析 (sdet_find_connected_components)
  3. 去掉面积<5的噪声连通域
  4. 加权质心 + 等效半径 + 圆度(4π×area/perimeter²) > 0.3 的保留
  5. 3σ截断面积异常大的 (面积 > median + 3×MAD)
  6. 去重: 与正常检测星距离<2px的饱和星不重复输出
- **输出排序**: 饱和星在前(flux=-1.0, saturated=1), 正常PSF星在后(saturated=0)
- **Python新增**: `detect_ex()` 返回 (coords, fluxes, saturated), `detect_debug_image()` 生成调试图像
- **调试图像**: 正常星=绿色十字, 饱和星=红色圆圈
- **测试结果**: 163颗饱和星 + 13936颗正常星, 耗时16.8s

### 2026-06-04 自适应fitRadius
- **新增功能**: fitRadius=0触发自动模式，C++内部基于连通域大小估算FWHM并自动调整拟合半径
- **算法**:
  1. 统计候选连通域像素数中位数 `med_pixel_count`
  2. 估算FWHM: `FWHM_est = sqrt(med_pixel_count / π) × 0.87` (Moffat4因子)
  3. 计算自适应半径: `fitRadius = max(6, min(20, 3 × FWHM_est))`
- **实测效果** (4500×3600银心图像):
  - fitRadius=0 (自动): actual=6, 检测35805星, 成功率53.8%, 耗时2.46s
  - fitRadius=6 (固定): 检测35805星, 成功率53.8%, 耗时2.44s
  - fitRadius=12 (固定): 检测22919星, 成功率34.0%, 耗时9.27s
- **关键发现**: fitRadius过大反而降低拟合成功率（采样区包含过多背景噪声）
- **实现位置**: sdet_api.cpp三个detect函数（sdet_detect, sdet_detect_debug, sdet_detect_ex）
- **Python接口**: 无需修改，fitRadius=0传递到C++触发自动计算
