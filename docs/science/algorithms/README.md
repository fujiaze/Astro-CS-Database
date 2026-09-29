# algorithms

本目录是算法推导层：把 docs/science/ 的公式落实为逐步推导、实现级算法合同与机器可校验门表，逐符号锚定到代码。

## 职责边界

- 放：算法推导、实现级算法合同（ALG-* 条款）、门与容差表、文档—代码锚的机器校验。
- 不放：科学公式正本（在 docs/science/）；模块工作细节（在 docs/plugins/）；工程检查器实现（在 eng/）；文献查证（在 docs/research/ 与 docs/references/）。

## 内容

- `CALIBRATION_ALGORITHMS.md` / `COSMETIC_ALGORITHMS.md` —— 定标与坏点修复算法推导。
- `STAR_DETECTION_ALGORITHMS.md` / `STAR_PSF_ALGORITHMS.md` —— 星点检测与 PSF 拟合算法推导。
- `PLATESOLVE.md` / `PHOTOMETRIC_FIT.md` —— 天测解算与测光拟合算法推导。
- `NOISE_ESTIMATION.md` / `INTEGRATION_ALGORITHMS.md` —— 噪声估计与集成归约算法推导。
- `DRIZZLE_GEOMETRY.md` / `HEALPIX_MAPPING.md` —— Drizzle 几何与 HEALPix 球面映射算法。
- `HIPS_WRITER.md` —— HiPS 写出算法与格式合同。
- `GAIA_QUERY.md` —— Gaia 星表查询与匹配算法。
- `ACR_EQUIVALENCE.md` —— ACR 等价性算法与边界。
- `GATES_AND_TOLERANCES.md` —— 星检测/PSF/WCS 门表与 SNR 定义（机器可校验事实源）。
- `PHASE2_COVERAGE.md`、`PHASE2_SESSION.md`、`PHASE2_SAMPLER.md`、`PHASE2_UPM_IMPL.md`、`PHASE2_REJECTION.md`、`PHASE2_INTEGRATION.md`、`PHASE2_MOSAIC_WRITE.md` —— mosaic 阶段的覆盖、会话装配、采样、UPM、排异、集成与写出算法。
- `UPM_SOLVER.md` —— UPM 求解器算法（稀疏平面/阻尼迭代）。
- `REJECTION_ALGORITHMS.md` —— 排异算法族推导与合成 Oracle。
- `PHASE3_PROJ_IMPL.md`、`PHASE3_RESAMPLE.md`、`PHASE3_RSMP_IMPL.md`、`PHASE3_FITS_IMPL.md` —— export 阶段的投影、重采样与 FITS 写出算法合同。
- `anchors/` —— 文档—代码符号/行锚合同、锚登记与机器校验脚本。

## 上游

上游：docs/ASTROCS_DESIGN.md §1.4（非目标）、§2.1–§2.2（创新点）、§3.3（科学量与星表）、§4.2（Phase1 节点流程）、§4.4（输出合同）、§5（mosaic 各节）、§6（export 各节）、§10（I/O 与原子产品）、§12.1（科学正确性与三重佐证）。

科学断言以 docs/science/ 为权威，佐证要求见 docs/engineering/DOCUMENT_GOVERNANCE.md §2。
