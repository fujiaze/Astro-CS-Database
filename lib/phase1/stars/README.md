# phase1/stars — StarDetector (L2 模块 README)

- 合同: `P1-003` / SCI-PSF-001 / SCI-PHOT-001 (docs/contracts/INDEX.yaml)
- Header: `lib/phase1/stars/star_detector.h`
- Source: `lib/phase1/stars/star_detector.cpp`
- Test: `tests/unit/p1_stars_test.cpp` (7 组: 孤立/重叠/饱和/边缘/纯噪声/tie-breaker/catalog)

## 职责
局部峰检测 + 质心/二阶矩 (FWHM/ellipticity) + sigma-clip 背景估计。
去重: flux 降序 + tie breaker (更左优先)。质量位: 1=饱和 2=边缘 4=重叠。
Catalog: 坐标(px)/flux(ADU)/FWHM(px)/SNR/质量位/id。

## 合同要点 (不抄完整公式)
- 5σ 检测阈值; 纯噪声无显著误报。
- 公式与单位见 SCI-PSF-001 / SCI-PHOT-001。

## P2 性能改动 (2026-09-14, 输出逐位不变)

- **PSF-BG-001**: `estimate_background` 的"每轮整图 `std::sort`"(16.2 M 像素 ×
  5 次)改为 `std::nth_element` 分位选择。`nth_element` 保证位置 k 的元素 ==
  整序后该位置元素, 故 median/MAD/bg **逐位等于旧实现**(非近似直方图) ——
  这是硬约束: 近似分位会移动 5σ 阈值 → 候选集变 → 下游 `p1_sources.json` /
  `p1_flux.json` 变。实测 4500×3600 真实帧 bg 估计 7.69 s → 见 P2 REPORT §3。
- **PSF-DET-001**: 局部峰扫描行并行 + 每线程本地缓冲合并 (OpenMP; 线程数由
  Runtime 注入的 OpenMP 环境决定, 本库不硬编码)。确定性依据: 排序键
  (val 降序 → x 升序 → y 升序) 在 (x,y) 唯一时构成**全序**, `std::sort` 输出与
  输入顺序无关 ⇒ catalog 逐位与线程数无关。无 OpenMP 构建时 `#pragma omp` 被
  忽略 → 串行回退, 结果相同。
- 构建接线: root `CMakeLists.txt` 的 `astrocs_phase1_stars` 链接
  `OpenMP::OpenMP_CXX` (仅 UN*X + OpenMP 可用时)。
- 调用方口径: Phase1 `star-psf` 节点 (`lib/core/src/module_adapters.cpp`
  `p1_op_star_psf`, P1-003 桥接类) —— **本类不是 lib/star_detector 的 sdet**。

