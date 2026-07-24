# Drizzle 2x 采样 + 逆 Drizzle 可逆性验证

## 背景与问题

### 当前问题
浏览器渲染 .hiss 出现摩尔纹，根因分析：
1. **欠采样**：当前 .hiss 用 nside=8192 (25.7"/px)，远粗于 T4 原图 6.19"/px，drizzle 实际是降采样
2. **pixfrac=0.8 稀疏填充**：每像素只填充 80% 区域，留 36% 缝隙（浏览器已用外扩 1.25 临时缓解）
3. **无逆 drizzle**：无法验证 drizzle 的可逆性

### 目标
1. 用 2x drizzle (nside=65536, 3.22"/px) 重新生成 .hiss，满足奈奎斯特采样
2. 实现逆 drizzle（HEALPix → 2D 图像，面积加权）
3. 完整往返验证：数值精度 + 星点保持 + WCS 一致性

## 技术参数

### 数据源
- T4 原图: `output/pipeline_debug/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red/2a_platesolve_solve.fits`
- 分辨率: 6.19"/px (理论) / 6.31"/px (CD 矩阵实测)
- 尺寸: 4500×3600
- 视场: 7.88° × 6.31°
- WCS: TAN-SIP 投影, CRVAL=(272.826, -13.132), CRPIX=(2250.5, 1800.5)

### 正向 drizzle 参数
- nside = 65536 (HEALPix 像素边长 3.22"/px, 2x 密度于 T4)
- nested = True
- pixfrac = 0.8 (保持通量守恒)
- 输出: .hiss 文件

### 逆 drizzle 参数
- 输入: .hiss (nside=65536)
- 输出: 2D numpy 数组 (4500×3600, 与原图同尺寸)
- WCS: 复用 T4 原图 WCS (CD + SIP)
- 算法: 方案 A (2D 驱动, 面积加权)

### 通量守恒修正 (关键 bug)
**当前实现 (drizzle_engine.cpp 第 267-302行)**:
- `originalPixelArea = pixelArea / pixfrac²` (恢复未收缩面积)
- `weight = overlapArea / originalPixelArea`
- `sum(weight per source) = A_shrunk / A_pixel = pixfrac²`
- `brightness = sumFlux` (不归一化)
- 结果: `sum_out = sum_in × pixfrac²` (**非标准, 通量不守恒**)

**标准 Drizzle (需修正为)**:
- `weight = overlapArea / A_shrunk` (占收缩后面积比例, sum=1)
- `sumFlux += pixelValue × weight`
- `brightness = sumFlux` (不归一化)
- 结果: `sum_out = sum_in` (**通量守恒**)

**物理意义**:
- pixfrac<1 收缩源像素覆盖范围, 但总通量不变
- 收缩后单位面积通量 = pixelValue / pixfrac² (能量提高)
- 标准 Drizzle: out = sum(in × overlap/A_shrunk), sum_out = sum_in

**验证指标**: sum(HEALPix brightness) / sum(源 pixelValue) 应 = 1.0 (±0.01)

### 浏览器渲染修正
- 撤销外扩 1.25 (基于错误假设 pixfrac 产生缝隙)
- 改为小膨胀 1.02 (仅覆盖浮点误差导致的亚像素缝隙)
- drizzle 加权积分本身无像素缝隙 (每个 HEALPix 像素接收多个源像素加权贡献)

## 实现方案

### 1. 删除现有 .hiss
- 删除 `output/pipeline_debug/.../drizzle/*.hiss`
- 删除 `*.ahpx` (旧格式)

### 2. 正向 drizzle (复用现有 hp_drizzle_run)
- 调用 `hp_drizzle_run(frame, nside=65536, nested=True, pixfrac=0.8, output_path)`
- 输入: T4 platesolve 后的 PipelineFrame
- 输出: .hiss 文件
- 验证: hiss_read 读取确认 nside/n_pix/pixel 统计

### 3. 逆 drizzle 实现 (新增)

#### 3.1 算法 (方案 A: 2D 驱动, 面积加权)
```
对目标 2D 图像每个像素 (px, py):
  1. 取像素四角 (px±0.5, py±0.5)
  2. pixfrac 收缩 (与正向对称, pixfrac=0.8)
  3. WCS+pixelToSky 映射四角到天球坐标
  4. queryDisc 查找覆盖的 HEALPix 像素
  5. 切平面面积裁剪 (PolyClip)
  6. 面积加权分配: out[py,px] = sum(healpix_value * overlap_area) / sum(overlap_area)
```

#### 3.2 实现位置
- C++: `lib/healpix_db/healpix_drizzle/drizzle_engine.h/.cpp` 新增 `inverse_drizzle` 方法
- Python 绑定: `healpix_drizzle.py` 新增 `hp_inverse_drizzle` 函数
- C API: `hp_drizzle_api.h` 新增 `hp_inverse_drizzle` 导出

#### 3.3 关键依赖 (已就绪)
- WcsSip::pixelToSky (正向, 像素→天球) ✓
- WcsSip::skyToPixel (逆向, 天球→像素) ✓ (备用)
- HealpixCore::radec2pix / pix2radec ✓
- HealpixCore::queryDisc ✓
- PolyClip::gnomonicForward / clipPolygon / polygonArea ✓
- hiss_read (healpix_io.dll) ✓

### 4. 验证脚本

#### 4.1 数值精度
- 原图 vs 重建图像素值对比
- 指标: RMS, MAE, 通量比 (sum(重建)/sum(原图))
- 可视化: 差值图直方图

#### 4.2 星点保持
- 原图星点检测 (亮度前 N 颗)
- 重建图书点检测 (同阈值)
- 对比: 质心位置偏差 (px), FWHM 变化, 峰值比

#### 4.3 WCS 一致性
- 原图像素 (px,py) → pixelToSky → (ra,dec) → radec2pix → (ipix) → pix2radec → (ra',dec') → skyToPixel → (px',py')
- 指标: 像素往返偏差 (px), 天球往返偏差 (arcsec)

## 验收标准
1. 正向 drizzle 生成 nside=65536 的 .hiss, n_pix > 0
2. 逆 drizzle 输出 4500×3600 numpy 数组, 无 NaN
3. 数值精度: 通量比 0.95~1.05, RMS < 原图 RMS 的 20%
4. 星点保持: 质心偏差 < 1 px, FWHM 变化 < 20%
5. WCS 一致性: 像素往返偏差 < 0.1 px, 天球往返偏差 < 0.5"

## 文件清单
- 修改: `lib/healpix_db/healpix_drizzle/drizzle_engine.h/.cpp` (新增 inverse_drizzle)
- 修改: `lib/healpix_db/healpix_drizzle/hp_drizzle_api.h/.cpp` (新增 C API)
- 修改: `lib/healpix_db/healpix_drizzle/healpix_drizzle.py` (新增 Python 绑定)
- 新建: `lib/healpix_db/healpix_drizzle/tests/test_round_trip.py` (往返验证)
- 删除: `output/pipeline_debug/.../drizzle/*.hiss`, `*.ahpx`
