# Module: dynamic_psf

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 本页为模块说明页；PSF 模块合同页 =
> lib/algorithms/psf/README.md（CONTRACT_READY）+ lib/algorithms/psf/
> module.yaml（astrocs.p1.psf，迁移目标 astrocs_p1_psf.dll）；冻结合同
> SCI-P1-PSF-001 / ALG-STARPSF-001（STAR_PSF_ALGORITHMS §11）/ DATA-P1-PSF
> （DATA_SEMANTICS §15）/ API-PSF-001（PUBLIC_API）；测试设计
> TEST-PSF-DESIGN-001；现状缺陷登记见 STAR_PSF_ALGORITHMS §11。本页其余章节
> 以 co-located README 为准。

## 职责

Moffat4 动态 PSF 建模与拟合质量代理（q_psf/residual_scale）。

## 非职责

不产生像素噪声权重；不进 Phase2 science weight（SNR-008 边界）。

## Public API

`lib/algorithms/psf/include/dynamic_psf.h`：参数结构 `DPSFFitParams{fitRadius,maxIter,tolerance}`、结果结构 `DPSFFitResult{status,B,A,cx,cy,sx,sy,theta,fwhm_x,fwhm_y,mad,flux,eccentricity}`；入口 `dpsf_fit`（uint16）/`dpsf_fit_batch`/`dpsf_fit_batch_f` 与 `dpsf_fit_batch_f32`/`dpsf_fit_batch_f64`/`dpsf_fit_batch_d`；结果整组释放 `dpsf_free_results`；状态码 `DPSF_FIT_OK/NO_CONVERGENCE/INVALID_PARAMS/ITERATION_LIMIT`。输出 PSF 块 [N,9] 契约。

## Data contract

输入星点裁剪图像（uint16 或 float 像素域，行主序）；输出 `DPSFFitResult` 参数/残差列（Moffat4 形状参数 + 拟合质量代理 q_psf/residual_scale，编排序列化为 PSF 块 [N,9]）。

## Ownership

工作区 RAII；输出块调用方分配。

## Thread safety

按星点并行；拟合独立。

## Errors

拟合不收敛 → 显式状态。

## Science IDs

SCI-PSF-001；ALG-STARPSF-*。

## Tests

合成 Moffat 恢复（PSF-001..008）；残差 Gaussian 假设。

## Source files

lib/algorithms/psf/。
