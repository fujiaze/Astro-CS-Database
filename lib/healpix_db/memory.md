# healpix_db - 模块开发memory

## 模块职责
Healpix天球分块数据库，提供LOD金字塔分层、球面浏览器可视化与Drizzle重投影功能，支持大规模天文图像的天球投影与多帧叠加。

## 当前版本
- 版本号：v1.0
- 最新commit：7129e32
- 更新时间：2026-07-12

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Healpix-Database
- 默认分支：main
- 关联仓库：
  - Healpix-Mosaic（healpix_stack）：多帧叠加合成
  - Healpix-Drizzle（healpix_drizzle）：Drizzle重投影

## 依赖列表
- C++17, OpenMP
- astro_image_io.dll（PipelineFrame命名块容器 + FITS读写）
- PyQt5 + vispy（球面浏览器可视化界面）

## 关键决策记录
- **模块拆分为3个独立仓库**：Healpix-Database核心、Healpix-Mosaic叠加、Healpix-Drizzle重投影，职责单一便于独立维护
- **hp_drizzle_run命名块直通**：Drizzle执行入口直接对接PipelineFrame命名块容器，避免临时文件中转，与管线引擎无缝集成
- **LOD金字塔分层**：天球分块按层级组织（LOD 0/1/2...），按视口动态加载，支持大规模图像高效浏览
- **球面浏览器基于vispy**：GPU加速渲染天球分块，支持缩放、平移、滤镜切换

## 进度日志
### 2026-07-12 hp_drizzle_run命名块直通完成
- 完成hp_drizzle_run命名块直通接口，对接PipelineFrame
- Drizzle重投影无需临时文件，直接从命名块读输入、写输出
- 推送至GitHub：commit 7129e32
