# Module: photometric_calib

> P1-PHOT-DOC 事实修订（2026-09-07，wave W1）：本页由源码核对后修订——
> 模块合同冻结落位 lib/photometric_calib/README.md（r1，CONTRACT_READY）+
> module.yaml（astrocs.p1.photometry，迁移目标 astrocs_p1_photometry.dll，
> entrypoint=MISSING）；合同 ID=SCI-PHOT-001 / ALG-PHOT-001..002
> （PHOTOMETRIC_FIT §13 逐符号锚）/ DATA-P1-PHOT（DATA_SEMANTICS §14）/
> API-PHOT-001（PUBLIC_API，photometric_calib.h 6 导出符号）；算法为
> 双向最近邻唯一配对（KD-tree，2.0px）+ 星等预过滤 + IRLS/Tukey 稳健
> 零点 scale=10^(−location)——本页旧文"合成星表相对流量→Photometric
> CalibrationQuality"与"参考星不足→NO_DATA"按 README §2/§6 修订（参考星
> 不足为退化 scale=1.0 rc=0，QA 结构体落位 snr_estimator）；现状构建=
> cpp/Makefile:11 + build.ps1:9（photometric_calib.dll），未编入根 CMake
> 主构建；lib/phase1/photometry（Photometer aperture 旧符号）合同并入
> README §9。DISP-PHOT-001..009 见 PHOTOMETRIC_FIT §13.3。

## 职责

测光定标（合成星表相对流量）→ PhotometricCalibrationQuality。

## 非职责

不产生逐像素 ivar（边界见 docs/science/PHOTOMETRY.md）。

## Public API

photometric_calib DLL（flux_calibrator）；C ABI 见 `lib/photometric_calib/cpp/include/photometric_calib.h`（`PC_API`/`extern "C"` 不抛异常，`gaia_client_handle` opaque borrow/不持有，`spec_stars`/`spectra_buf` 本调用内 `free`，`out_*` 调用方分配/释放）— 契约锚 `docs/contracts/PUBLIC_API.md` + `docs/standards/C_ABI_STANDARD.md`。

## Data contract

仪器/合成流量 → dex 残差 + sigma_mag/sigma_cal_rel。

## Ownership

输出结构调用方释放。

## Thread safety

帧级串行；无共享状态。

## Errors

输入校验失败 → 负返回码（API-PHOT-001）；参考星/PSF 星/光谱星不足或滤光片
失败 → 退化恒等校正（scale=1.0、rc=0、diag/records 显式登记，README §6）。

## Science IDs

SCI-PHOT-001（docs/science/PHOTOMETRY.md，FROZEN）；ALG-PHOT-001..002；
DATA-P1-PHOT（DATA_SEMANTICS §14）；API-PHOT-001；SRC-PHOT-001；
TEST-PHOT-DESIGN-001（PHOTOMETRIC_FIT §13.4，冻结容差）。

## Tests

冻结测试设计 TEST-PHOT-DESIGN-001（fixture F1-F6/不变量 I1-I6/负面矩阵/
SCI-PHOT-001 §11 容差：注入 rtol 1e-4、20% 离群 Δlocation<0.1 dex、
NumPy rtol 1e-9）；可执行 TEST-P1-PHOT-001 由 P1-PHOT-TEST 落地；现状
既有锚 tests/unit/p1_wcs_phot_test.cpp（Photometer 4 组）+
lib/photometric_calib/cpp/test/test_photometric_calib.py（旧测，对齐重锚）。

## Source files

lib/photometric_calib/。
