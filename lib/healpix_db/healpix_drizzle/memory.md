# Healpix-Drizzle 模块记忆

## 模块职责
球面 Drizzle 重投影引擎，将 WCS 投影图像重投影到 HEALPix 等积网格。

## 当前版本
v1.0

## GitHub 仓库
- 远程地址: https://github.com/fujiaze/Healpix-Drizzle-Cpp
- 默认分支: main

## 依赖列表
- C++17
- OpenMP
- astro_image_io.dll (AIO，提供 I/O 与压缩 API)
- healpix_core (HEALPix 球面运算)

## 关键决策记录

### 1. 6步 Drizzle 流水线
取四角 → pixfrac 收缩 → SIP+WCS 映射 → HEALPix 邻域检索 → 切平面面积裁剪 → 通量守恒分配

### 2. SIP 多项式畸变校正
- A/B 前向映射 + AP/BP 逆向映射
- 最多支持 4 阶多项式
- 同时支持 TAN (gnomonic) 投影

### 3. Sutherland-Hodgman 多边形裁剪
- 切平面面积裁剪保证严格通量守恒
- 输入像素多边形与 HEALPix 像素边界求交

### 4. hp_drizzle_run 命名块直通接口
- 对接 PipelineFrame 内存结构，避免临时文件落地
- 直接访问 PipelineFrame.data / width / height / wcs 字段
- OpenMP 16 线程并行处理

## 进度日志
- 2026-07-12: hp_drizzle_run 命名块直通接口完成，对接 PipelineFrame 实现零临时文件
- 2026-07-13: 建立 git 仓库并关联 GitHub 远程 (Healpix-Drizzle-Cpp)
- 2026-07-13: 输出格式从 .ahpx 改为 .hiss (Task 4)
  - drizzle_engine: writeAhpx → writeHis, 调用 healpix_io.dll 的 hiss_write
  - 新增 DrizzleMeta 结构体 (filter/exposure_s/obs_time/fits_meta)
  - hp_drizzle_run 从 header KV 读取 FILTER/EXPTIME/DATE-OBS + 16个 fits_meta KV
  - output_path .ahpx 后缀自动改 .hiss (向后兼容)
  - Makefile 添加 healpix_io 依赖 (-I../healpix_io/include -L../healpix_io -lhealpix_io)
  - 测试 3/3 通过 (含 hiss_read 往返验证 filter=Lum/n_pix=178/fits_meta.IMAGETYP=LIGHT)
- 2026-07-14: 黑色缝隙修复 (三项综合优化)
  - **根因1**: 候选像素覆盖不全 - 仅中心1-ring 在 WCS 畸变下可能漏掉源像素四角延伸到的相邻像素
  - **根因2**: HEALPix 像素四角近似为方形, 但实际是菱形, 边缘判定不准
  - **根因3**: pixfrac=0.8 收缩源像素导致相邻源像素之间有固有缝隙
  - **修复1**: 候选像素从仅中心1-ring 扩到 5基准全部1-ring (5基准+各8邻居=45候选, 数组扩到48)
  - **修复2**: HEALPix 像素四角改为菱形对角计算
    - NS 半对角线 = sqrt(sqrt(3))/2 * res ≈ 0.658 * res
    - EW 半对角线 = 1/sqrt(sqrt(3)) * res ≈ 0.760 * res
    - 4 顶点 (北/西/南/东, 逆时针顺序, 兼容 Sutherland-Hodgman 算法)
    - 关键教训: PolyClip::clipPolygon 的 Sutherland-Hodgman 要求裁剪多边形逆时针, 顺时针会导致所有 subject 点被误判为外侧 → 0 像素输出
  - **修复3**: 默认 pixfrac 从 0.8 提到 1.0 (drizzle_engine.h / hp_drizzle_api.h / pipeline_adapter.py / healpix_drizzle.py / run_forward_drizzle.py)
  - **性能**: 47.1s/帧 (16.2M源像素 → 61.6M HEALPix像素), 比 33.7s(仅中心1-ring+pixfrac=0.8) 慢, 但消除固有缝隙
  - **缝隙诊断**: 邻居缺失率 1.39% → 1.34%, 94.7%像素无缺失邻居, 剩余1.34%全部在图像边缘 (ra=276.52~276.91, 全图ra上限276.90), 为边界效应无法算法消除

## 教训记录
- **菱形顶点顺序**: Sutherland-Hodgman 多边形裁剪算法要求裁剪多边形逆时针, 顶点顺序错误 (顺时针) 会导致所有 subject 点被误判为外侧, 返回空交集, 表现为 0 像素输出
- **HEALPix 像素形状**: 赤道带 HEALPix 像素是菱形 (diamond), 不是方形. 之前用 (±half_ra, ±half_dec) 方形近似在边缘会误判不相交, 导致微小黑色缝隙
- **pixfrac<1 固有缝隙**: pixfrac<1.0 收缩源像素覆盖范围, 相邻源像素之间必然有间隙, 对应 HEALPix 像素无源像素命中, 形成球面空缺

### 11.4 Drizzle 黑色缝隙修复（2026-07-14）（2026-07-15，从 PROJECT_ARCHITECTURE.md 迁入）

**问题**：drizzle 输出在球面渲染上呈现微小黑色缝隙。

**根因分析**:
1. 候选像素覆盖不全 - 仅中心1-ring 在 WCS 畸变下可能漏掉源像素四角延伸到的相邻像素
2. HEALPix 像素四角近似为方形, 但实际是菱形, 边缘判定不准
3. pixfrac=0.8 收缩源像素导致相邻源像素之间有固有缝隙

**修复**:
- **候选像素**: 仅中心1-ring → 5基准（中心+四角）各取1-ring邻居（5+5×8=45候选，数组扩到48）
- **HEALPix 像素四角**: 方形近似 → 菱形对角计算
  - NS 半对角线 = `sqrt(sqrt(3))/2 * res ≈ 0.658 * res`
  - EW 半对角线 = `1/sqrt(sqrt(3)) * res ≈ 0.760 * res`
  - 4 顶点（北/西/南/东，**逆时针**顺序，兼容 Sutherland-Hodgman 算法）
- **pixfrac 默认值**: 0.8 → 1.0（drizzle_engine.h / hp_drizzle_api.h / pipeline_adapter.py / healpix_drizzle.py / run_forward_drizzle.py）

**关键教训**:
- Sutherland-Hodgman 多边形裁剪算法要求裁剪多边形 **逆时针**，顶点顺时针会导致所有 subject 点被误判为外侧 → 0 像素输出
- HEALPix 赤道带像素是菱形（diamond），不是方形。之前用 (±half_ra, ±half_dec) 方形近似在边缘会误判不相交，导致微小黑色缝隙
- pixfrac<1.0 收缩源像素覆盖范围，相邻源像素之间必然有间隙，形成球面空缺

**性能结果** (nside=65536, 16.2M源像素):

| 方案 | 候选数 | pixfrac | 耗时(s) | HEALPix 像素数 |
|------|--------|---------|---------|----------------|
| queryDisc BFS | 全 | 0.8 | 67.6 | 61.6M |
| 5基准全1-ring | ~45 | 0.8 | 43.5 | 61.6M |
| 仅中心1-ring | ~13 | 0.8 | 33.7 | 61.6M |
| 5基准全1-ring+菱形+pixfrac=1.0 | ~45 | 1.0 | 47.1 | 61.6M |

**缝隙诊断** (nside=65536, 采样5000像素):

| 指标 | 修复前 | 修复后 |
|------|--------|--------|
| 零值像素 | 0 / 61598882 (0.000%) | 0 / 61611427 (0.000%) |
| 邻居缺失率 | 1.39% | 1.34% |
| 无缺失邻居像素比例 | 94.6% | 94.7% |
| 缺失位置 | ra=276.48~276.91 (边缘) | ra=276.52~276.91 (边缘) |

**结论**: 算法层面已做三项修复，邻居缺失率从 1.39% 降至 1.34%，剩余 1.34% 全部在图像边缘（HEALPix 像素 8 邻居中部分落在源图像覆盖范围外），为边界效应无法算法消除。

## SNR 通道贯通 (2026-07-15)

### 断层1修复: drizzle 落盘保留 sumWeight/sumSnrSq
- **drizzle_engine.cpp**: PixelEntry 增加 snr 字段 (sqrt(sumSnrSq/sumWeight)); writeHis 构造 snrArr 并调用 hiss_write 写入 snr 通道
- **hp_drizzle_api.cpp**: hp_drizzle_run 从 PipelineFrame 读取 "snr" 块 (aio_frame_get_block), 验证类型/尺寸后传递 snrPtr 给 engine.drizzle (原传 nullptr 导致 snr 全为 1.0)
- **关键修复**: hp_drizzle_run 行 417 `nullptr` → `snrPtr`, 并在 data 块读取后增加 snr 块读取逻辑 (行 283-297)
- **端到端验证**: .hiss snr 通道 has_snr=True, 值分布合理 (min=0.23, max=9.71, median=3.12, std=0.86)
