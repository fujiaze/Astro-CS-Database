# Drizzle 2x 采样 + 逆 Drizzle 可逆性验证 - Checklist

## 阶段 1: 清理与正向 drizzle
- [ ] 1.1 删除 `output/pipeline_debug/.../drizzle/*.hiss`
- [ ] 1.2 删除 `output/pipeline_debug/.../drizzle/*.ahpx`
- [ ] 1.3 加载 T4 platesolve 后的 FITS 为 PipelineFrame
- [ ] 1.4 调用 hp_drizzle_run(nside=65536, nested=True, pixfrac=0.8)
- [ ] 1.5 hiss_read 验证: nside=65536, n_pix>0, pixel 统计合理
- [ ] 1.6 浏览器加载新 .hiss 确认渲染正常 (无摩尔纹/无缝隙)

## 阶段 2: 逆 drizzle 实现
- [ ] 2.1 DrizzleEngine::inverse_drizzle 方法声明 (drizzle_engine.h)
- [ ] 2.2 inverse_drizzle 实现 (drizzle_engine.cpp)
  - [ ] 2.2.1 加载 .hiss (hiss_read)
  - [ ] 2.2.2 遍历目标 2D 像素 (OpenMP 并行)
  - [ ] 2.2.3 像素四角 → pixelToSky → 天球坐标
  - [ ] 2.2.4 queryDisc 查找覆盖的 HEALPix 像素
  - [ ] 2.2.5 PolyClip 切平面面积裁剪
  - [ ] 2.2.6 面积加权分配: out = sum(value*area) / sum(area)
  - [ ] 2.2.7 处理无覆盖像素 (输出 0 或 NaN)
- [ ] 2.3 C API: hp_inverse_drizzle (hp_drizzle_api.h/.cpp)
- [ ] 2.4 Python 绑定: hp_inverse_drizzle (healpix_drizzle.py)
- [ ] 2.5 编译验证 (dll 生成, 符号导出)

## 阶段 3: 往返验证
- [ ] 3.1 正向: T4 FITS → .hiss (nside=65536)
- [ ] 3.2 逆向: .hiss → 2D numpy 数组 (4500×3600)
- [ ] 3.3 数值精度
  - [ ] 3.3.1 RMS, MAE 计算
  - [ ] 3.3.2 通量比 (sum(重建)/sum(原图))
  - [ ] 3.3.3 差值图直方图
- [ ] 3.4 星点保持
  - [ ] 3.4.1 原图星点检测 (亮度前 100 颗)
  - [ ] 3.4.2 重建图书点检测 (同阈值)
  - [ ] 3.4.3 质心位置偏差 (px)
  - [ ] 3.4.4 FWHM 变化
  - [ ] 3.4.5 峰值比
- [ ] 3.5 WCS 一致性
  - [ ] 3.5.1 像素→天球→HEALPix→天球→像素 往返
  - [ ] 3.5.2 像素往返偏差 (px)
  - [ ] 3.5.3 天球往返偏差 (arcsec)

## 阶段 4: 报告与文档
- [ ] 4.1 生成验证报告 (JSON + Markdown)
- [ ] 4.2 更新 drizzle 模块 memory.md
- [ ] 4.3 更新根 memory.md
- [ ] 4.4 更新 PROJECT_ARCHITECTURE.md
- [ ] 4.5 推送 drizzle 仓库到 GitHub

## 验收标准
- [ ] 通量比 0.95~1.05
- [ ] RMS < 原图 RMS 的 20%
- [ ] 星点质心偏差 < 1 px
- [ ] FWHM 变化 < 20%
- [ ] 像素往返偏差 < 0.1 px
- [ ] 天球往返偏差 < 0.5"
- [ ] 浏览器渲染无摩尔纹
