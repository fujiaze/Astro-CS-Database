# Module: dynamic_psf

> P1-PSF-DOC 增补（2026-09-07）：legacy 模块页追加说明段；PSF 模块合同页 =
> lib/dynamic_psf/README.md（r1，CONTRACT_READY）+ lib/dynamic_psf/
> module.yaml（astrocs.p1.psf，迁移目标 astrocs_p1_psf.dll）；冻结合同
> SCI-P1-PSF-001 / ALG-STARPSF-001（STAR_PSF_ALGORITHMS §11）/ DATA-P1-PSF
> （DATA_SEMANTICS §15）/ API-PSF-001（PUBLIC_API）；测试设计
> TEST-PSF-DESIGN-001；现状缺陷 DISP-PSF-001..006。本页其余章节为历史
> 描述，以 co-located README r1 为准。

## 职责

Moffat4 动态 PSF 建模与拟合质量代理（q_psf/residual_scale）。

## 非职责

不产生像素噪声权重；不进 Phase2 science weight（SNR-008 边界）。

## Public API

dynamic_psf DLL（PSF 拟合/评估）；PSF 块 [N,9] 契约。

## Data contract

星点裁剪图像 → 参数/残差列。

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

lib/dynamic_psf/。
