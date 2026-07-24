# Drizzle 2x 采样 + 逆 Drizzle 可逆性验证 - Tasks

## Task 1: 清理与正向 drizzle
**文件**: `output/pipeline_debug/.../drizzle/`, `lib/healpix_db/healpix_drizzle/healpix_drizzle.py`
**步骤**:
1. 删除 `output/pipeline_debug/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red/drizzle/*.hiss`
2. 删除同目录 `*.ahpx`
3. 写 Python 脚本: 加载 `2a_platesolve_solve.fits` 为 PipelineFrame
4. 调用 `hp_drizzle_run(frame, nside=65536, nested=True, pixfrac=0.8, output_path)`
5. hiss_read 验证: nside=65536, n_pix>0, pixel min/max/mean
6. 浏览器加载新 .hiss 确认渲染
**验证**: nside=65536, n_pix>0, pixel 统计合理, 浏览器无摩尔纹

## Task 2: 逆 drizzle C++ 实现
**文件**: `lib/healpix_db/healpix_drizzle/drizzle_engine.h`, `drizzle_engine.cpp`
**步骤**:
1. DrizzleEngine 类新增 `inverse_drizzle` 方法声明:
   ```cpp
   int inverse_drizzle(const char* hiss_path,
                       const WcsSip& target_wcs,
                       int width, int height,
                       double pixfrac,
                       std::vector<float>& out_image,
                       std::vector<float>& out_weight);
   ```
2. 实现:
   - hiss_read 加载 .hiss (ipix + pixel 数组)
   - 构建 ipix→pixel 哈希表 (O(1) 查找)
   - OpenMP 16 线程遍历目标 2D 像素 (py, px)
   - 每像素:
     - 取四角 (px±0.5, py±0.5)
     - pixfrac 收缩 (与正向对称)
     - pixelToSky 映射四角到天球
     - queryDisc 查找覆盖的 HEALPix 像素
     - PolyClip 切平面面积裁剪
     - 面积加权: out = sum(hp_val*area) / sum(area)
   - 无覆盖像素: out=0, weight=0
3. 处理边界: 像素四角超出图像范围时跳过
**验证**: 编译成功, 函数可调用, 输出 4500×3600 数组

## Task 3: C API + Python 绑定
**文件**: `hp_drizzle_api.h`, `hp_drizzle_api.cpp`, `healpix_drizzle.py`
**步骤**:
1. C API:
   ```c
   int hp_inverse_drizzle(const char* hiss_path,
                          const char* fits_path,  // 提取 WCS
                          double pixfrac,
                          float* out_image,       // width*height
                          float* out_weight,
                          int width, int height);
   ```
2. Python 绑定:
   ```python
   def hp_inverse_drizzle(hiss_path, fits_path, pixfrac=0.8) -> tuple[np.ndarray, np.ndarray]:
       # 返回 (image, weight), shape=(height, width)
   ```
3. 从 fits_path 提取 WCS (CD + SIP) 构造 WcsSip
4. 输出 numpy 数组 (shape=height×width, dtype=float32)
**验证**: Python 可调用, 返回正确 shape 的数组

## Task 4: 往返验证脚本
**文件**: `lib/healpix_db/healpix_drizzle/tests/test_round_trip.py`
**步骤**:
1. 正向: T4 FITS → .hiss (nside=65536)
2. 逆向: .hiss → 2D numpy (4500×3600)
3. 数值精度:
   - RMS = sqrt(mean((orig - recon)^2))
   - MAE = mean(|orig - recon|)
   - 通量比 = sum(recon) / sum(orig)
   - 差值图直方图 (matplotlib)
4. 星点保持:
   - 原图星点检测 (简单阈值 + 质心)
   - 重建图书点检测 (同阈值)
   - 匹配最近邻, 计算质心偏差 / FWHM / 峰值比
5. WCS 一致性:
   - 采样 1000 个像素 (px,py)
   - pixelToSky → radec2pix (ipix) → pix2radec → skyToPixel → (px',py')
   - 像素往返偏差 = |(px,py) - (px',py')|
   - 天球往返偏差 = greatCircleDistance((ra,dec), (ra',dec'))
6. 生成报告: JSON + Markdown
**验证**: 所有指标在验收标准内

## Task 5: 文档与推送
**文件**: `memory.md`, `PROJECT_ARCHITECTURE.md`, drizzle 仓库
**步骤**:
1. 更新 drizzle 模块 memory.md
2. 更新根 memory.md (只记使用方法/路径)
3. 更新 PROJECT_ARCHITECTURE.md (drizzle 章节加逆 drizzle)
4. 推送 drizzle 仓库到 GitHub
**验证**: git push 成功
