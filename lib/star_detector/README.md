# Star Detector — 天文图像星点检测器

从16bit天文图像中检测星点，支持正常星Moffat4 PSF拟合和饱和星半阈值检测，输出坐标+flux+饱和标记，可选输出拟合参数。

## 检测效果示例

![检测效果示例](test_output/example.jpg)

*绿色十字 = 正常PSF拟合星点，红色圆圈 = 饱和星点（半径=r）*

## 特性

- **全C++核心算法**：动态背景分离、连通域分析、Moffat4拟合、饱和星检测、去重、排序全部在C++中实现
- **Python仅作胶水层**：ctypes调用DLL + 结果可视化，不参与核心计算
- **16bit原生输入**：直接接收uint16图像数据，适配天文相机ADC
- **动态区域迭代阈值背景分离**：替代à-trous小波，自适应不同区域背景亮度，OpenMP并行化
- **自适应fitRadius**：基于连通域大小自动估算FWHM并调整拟合半径，fitRadius=0触发自动模式
- **Moffat4 PSF拟合**：解析雅可比+OpenMP并行化，200次LM迭代
- **候选预过滤**：像素数≤4、包围盒≥2×2、长宽比≤3，大幅减少无效拟合
- **半阈值饱和星检测**：原图(max+min)/2二值化→连通域→圆盘拟合
- **智能去重**：饱和星与正常星重叠时优先保留Moffat结果
- **有序输出**：饱和星在前按r降序，正常星按flux降序
- **可选参数输出**：支持fwhm_x, fwhm_y, sx, sy, theta, background, amplitude, r

## 检测流水线

```
输入: uint16图像 (通过Python传入，或直接调用C API)
  │
  ├─ 正常星检测流程 ──────────────────────────────
  ├─ 1. uint16→float32转换
  ├─ 2. 动态区域迭代阈值背景分离 → 细节层
  │     (100px基础块 + 20px异常块精细化 + 积分图O(1)掩膜填充 + OpenMP并行)
  ├─ 3. 细节层>0二值化
  ├─ 4. 连通域分析
  ├─ 5. 候选预过滤: 像素数≤4 / 包围盒<2×2 / 长宽比>3 → 丢弃
  ├─ 6. 批量Moffat4拟合 (OpenMP 16线程并行)
  ├─ 7. FWHM过滤: |fwhm-med| > fwhmClipSigma×MAD → 剔除
  ├─ 8. 圆度过滤: max(sx,sy)/min(sx,sy) > maxAxisRatio → 剔除
  │
  ├─ 饱和星检测流程 ──────────────────────────────
  ├─ 9. 半阈值二值化: threshold = (max+min)/2
  ├─ 10. 连通域分析 + 候选预过滤
  ├─ 11. 圆盘拟合: 加权重心 + 等效半径r=sqrt(count/π)
  │
  ├─ 合并输出 ────────────────────────────────────
  ├─ 12. 去重: 饱和星与正常星重叠<2px → 丢弃饱和星
  ├─ 13. 排序: 饱和星(按r降序)在前 + 正常星(按flux降序)在后
  │
  └─ 输出: x[], y[], flux[], saturated[], extras{}
```

## 数据结构

### SDetParams — 检测参数

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| structureLayers | int | 5 | 保留兼容，暂未使用 |
| hotPixelFilterRadius | int | 1 | 热像素中值滤波半径 |
| iterativeClipSigma | float | 9.0 | sigma-clip阈值倍数 |
| iterativeMaxRounds | int | 5 | sigma-clip最大迭代轮数 |
| medianFilterDetail | int | 1 | 是否对细节层做3×3中值滤波 |
| maxStars | int | 0 | 最大输出星点数，0=不限制 |
| fitRadius | int | 6 | PSF拟合采样区半径，0=自动模式（基于连通域大小估算FWHM） |
| fwhmClipSigma | float | 3.0 | FWHM剪裁sigma倍数 |
| maxAxisRatio | float | 2.0 | 最大轴比（长轴/短轴） |

### StarDetectionResult — Python返回结构

| 字段 | 类型 | 说明 |
|------|------|------|
| x | list[float] | 星点X坐标 |
| y | list[float] | 星点Y坐标 |
| flux | list[float] | 正常星=振幅A，饱和星=-1 |
| saturated | list[int] | 0=正常星，1=饱和星 |
| extras | dict[str, list[float]] | 可选额外参数 |

支持的额外参数名：`fwhm_x`, `fwhm_y`, `sx`, `sy`, `theta`, `background`, `amplitude`, `r`

额外参数填充规则：
- 正常星：Moffat拟合字段有效，`r`填0
- 饱和星：`r`有效，Moffat拟合字段填-1

## C API

```c
#include "star_detector.h"

// 创建/销毁
StarDetectorHandle sdet_create(const SDetParams *params);
void sdet_destroy(StarDetectorHandle handle);

// 基础检测：仅正常星，输出坐标
int sdet_detect(StarDetectorHandle handle,
                const uint16_t *image, int width, int height,
                double **out_x, double **out_y, int *out_count);
void sdet_free_coords(double *coords);

// 扩展检测：正常星+饱和星+可选参数
int sdet_detect_ex(StarDetectorHandle handle,
                   const uint16_t *image, int width, int height,
                   double **out_x, double **out_y,
                   float **out_flux, int **out_saturated, int *out_count,
                   const char **extra_names, int extra_count, float ***out_extras);
void sdet_free_detect_ex(double *x, double *y, float *flux, int *saturated,
                          float **extras, int extra_count);

// 调试检测：含调试图输出
int sdet_detect_debug(StarDetectorHandle handle,
                      const uint16_t *image, int width, int height,
                      double **out_x, double **out_y, int *out_count,
                      float **out_detail, float **out_smap, float **out_binary,
                      const char **extra_names, int extra_count, float ***out_extras);
void sdet_free_debug_maps(float *maps);
```

## Python使用

### 基础检测

```python
from star_detector import StarDetector, SDetParamsPy
import numpy as np

image = ...  # np.ndarray, uint16或float32(自动转换)

det = StarDetector()
coords = det.detect(image)  # 返回 [(x, y), ...]
print(f"检测到 {len(coords)} 颗星")
det.close()
```

### 扩展检测（含饱和星+可选参数）

```python
det = StarDetector()
result = det.detect_ex(image, extra_names=['fwhm_x', 'fwhm_y', 'r'])

print(f"总数: {result.count}, 饱和: {result.saturated_count}, 正常: {result.normal_count}")

# 饱和星在前，按r降序
for i in range(result.saturated_count):
    print(f"饱和星[{i}]: ({result.x[i]:.1f}, {result.y[i]:.1f}) r={result.extras['r'][i]:.1f}")

# 正常星在后，按flux降序
for i in range(result.saturated_count, result.count):
    print(f"正常星[{i}]: ({result.x[i]:.1f}, {result.y[i]:.1f}) flux={result.flux[i]:.1f}")

det.close()
```

### 调试图像输出

```python
det = StarDetector()
result = det.detect_debug_image(image, "debug_output.png", extra_names=['r'])
# 绿色十字=正常星，红色圆圈=饱和星(半径=r)
det.close()
```

## 算法详解

### 动态区域迭代阈值背景分离

替代之前的à-trous小波分解，自适应不同区域的背景亮度差异：

1. **100px基础块**：对每个100×100块做3轮sigma-clip，取med/mad的中位数（OpenMP并行）
2. **异常块检测**：均值/中位数比超出0.707~1/0.707范围的块标记为异常（含密集星点或星云）
3. **20px精细化**：异常块拆成20×20子块重新做sigma-clip
4. **星点掩膜**：像素值 > med + clip_sigma × mad 的标记为星点（OpenMP并行）
5. **积分图填充**：星点像素用周围非掩膜像素均值填充（O(1)查询，OpenMP并行）
6. **细节层** = 原图 - 背景层（OpenMP并行）

相比à-trous小波：
- 保留更多暗弱星点信号
- 自适应亮暗背景差异
- 异常区域精细化处理
- 全流程OpenMP并行化

### 候选预过滤

在Moffat4拟合前过滤无效候选，大幅减少拟合计算量：

1. **像素数≤4**：过小连通域为噪声
2. **包围盒<2×2**：单行/单列像素不是星点
3. **长宽比>3**：拉长的连通域为卫星轨迹/条纹等非星点

过滤效果：候选数从94000+降至72000+，拟合时间减少约30%。

### 自适应fitRadius

根据图像星点大小自动调整PSF拟合采样半径，无需手动设置：

**算法**：
1. 统计所有候选连通域的像素数中位数 `med_pixel_count`
2. 估算FWHM：`FWHM_est = sqrt(med_pixel_count / π) × 0.87`（Moffat4因子）
3. 计算自适应半径：`fitRadius = max(6, min(20, 3 × FWHM_est))`

**使用方式**：
- `fitRadius=0`：自动模式，C++内部计算最优半径
- `fitRadius>0`：固定模式，使用指定值

**实测效果**（4500×3600银心图像）：

| fitRadius设置 | 实际半径 | 检测星数 | 拟合成功率 | 耗时 |
|---------------|---------|---------|-----------|------|
| 0（自动） | 6 | 35805 | 53.8% | 2.46s |
| 6（固定） | 6 | 35805 | 53.8% | 2.44s |
| 12（固定） | 12 | 22919 | 34.0% | 9.27s |

**结论**：fitRadius过大反而降低拟合成功率（采样区包含过多背景噪声），自适应模式自动选择最优值。

### Moffat4 PSF拟合

Moffat4模型：`I(x,y) = B + A/(1+Q)^4`，其中 `Q = p1·dx² + 2p2·dx·dy + p3·dy²`

优化技术：
- **解析雅可比**：7个偏导公式，替代数值差分，每次LM迭代从8次残差计算降为1次
- **LMWorkspace预分配**：避免每次fit的malloc/free，每个OpenMP线程复用
- **200次LM迭代**：确保收敛
- **FWHM/圆度过滤**：剔除异常PSF

### 半阈值饱和星检测

饱和星PSF变形，无法用Moffat拟合。独立检测流程：

1. **半阈值二值化**：`threshold = (img_max + img_min) / 2`
2. **连通域分析**：提取高亮区域，过滤≤4px、包围盒<2×2、长宽比>3
3. **圆盘拟合**：加权重心(cx, cy) + 等效半径 `r = sqrt(pixel_count / π)`
4. **去重**：与正常星重叠<2px时丢弃饱和星（优先Moffat结果）

### 去重与排序

全部在C++中实现：

- **网格化查询**：2px网格加速邻近星点查找
- **饱和星vs正常星**：距离<2px → 丢弃饱和星
- **饱和星之间**：保留r较大的
- **正常星之间**：保留flux较大的
- **排序**：`std::stable_sort`，饱和星在前按r降序，正常星按flux降序

## 性能

测试环境：16线程CPU + 64GB内存

| 图像 | 分辨率 | 饱和星 | 正常星 | 总计 | 耗时 |
|------|--------|--------|--------|------|------|
| Red帧(银心) | 4500×3600 | 129 | 40912 | 41041 | ~9s |

各阶段耗时分布：

| 阶段 | 耗时 | 占比 |
|------|------|------|
| uint16→float | 17 ms | 0.2% |
| 动态背景分离 | 447 ms | 5.2% |
| 连通域分析+候选提取 | 178 ms | 2.1% |
| Moffat4拟合(16线程) | 7875 ms | 91.0% |
| 饱和星检测 | 37 ms | 0.4% |
| 去重+排序 | 19 ms | 0.2% |

主要瓶颈为Moffat4拟合，通过候选预过滤（像素数≤4/包围盒<2×2/长宽比>3）已将候选数从94000+降至72000+。

## 编译

**依赖**：MinGW-w64 g++ (C++17), OpenMP

```bash
make all
```

输出 `star_detector.dll`

**环境变量**：
- `STAR_DETECTOR_LOG_LEVEL`：日志级别（0=INFO, 1=DEBUG, 2=WARN, 3=ERROR），默认INFO

## 模块结构

```
lib/star_detector/
├── include/star_detector.h    # 公共C API头文件
├── src/
│   ├── sdet_api.cpp           # 检测流水线（detect/detect_ex/detect_debug）
│   ├── sdet_detector.cpp      # 连通域分析、去重算法
│   ├── sdet_image.cpp         # 图像处理（动态背景、中值滤波、积分图）
│   ├── sdet_background.cpp    # SExtractor背景估计
│   ├── sdet_log.cpp           # 日志系统
│   └── sdet_detector.h        # 内部头文件
├── python/star_detector.py    # Python封装（ctypes + 可视化）
├── test_output/               # 示例输出图像
└── Makefile
```

## 数据流

```
FITS/XISF文件
    │
    ├─ 方案A：Python胶水层（当前）
    │   Python (astro_image_io.dll) 读取 → numpy数组
    │   Python传给 star_detector.dll → C++处理 → 返回结果
    │   Python可视化（PIL绘制标注图）
    │
    ├─ 方案B：纯C++调用（可选）
    │   C++直接调用 astro_image_io C API 读取
    │   C++处理 → 输出结果
    │   （需在star_detector中链接astro_image_io）
    │
    └─ 输出：x[], y[], flux[], saturated[], extras{}
```

## 依赖

- **astro_image_io**（可选）：FITS/XISF图像读取，Python端使用
- **Dynamic-PSF**（算法同源）：Moffat4拟合引擎，Star Detector内联了拟合代码

## 参考文献

- **[SExtractor](https://github.com/astromatic/sextractor)**：网格化背景估计、sigma-clip、连通域分析
- **[PSFEx](https://github.com/astromatic/psfex)**：chi²筛选、FWHM/椭圆度过滤

核心算法已根据MIT许可重新实现，参考设计思路但代码完全独立。

## 许可

MIT License