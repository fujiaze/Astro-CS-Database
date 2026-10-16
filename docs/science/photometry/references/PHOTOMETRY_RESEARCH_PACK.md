# 测光方法的文献阅读

本页为[光度分册](../PHOTOMETRY.md)提供文献定位，按待核验的方法整理出处。科学公式、单位、产品和误差状态由光度正本拥有；书目信息、读过方法、参考实现与实验支持分别记录，本轮没有逐条重新读取外部原文。

## 按问题选择资料

| 问题 | 文献集合 | 核验时要确认的内容 |
|---|---|---|
| XP 表示与外定标 | [1]–[7] | 数据表示、实际通量单位、采样范围、外定标和验证域 |
| 合成测光与系统响应 | [8]–[13] | 光子/能量约定、QE、响应积分归一、零点和带宽 |
| 参考星等与绝对光谱尺度 | [14]–[19] | 各文献真实主题、星等系统、参考 SED 与零点的用途 |
| 引导测量与源/PSF | [20]–[29] | 几何求解、检测、模型拟合、孔径和实际生产职责的区别 |
| 噪声与最优测量 | [27]–[33] | 固定条件方差、背景自由度、相关噪声与模型失配 |
| 平场和相对光度 | [34]–[36]、[39]–[40] | 可辨识的参数乘积、共享来源、空间支持与系统误差 |
| 背景、稳健统计和求解 | [22]、[37]–[38]、[42]–[44] | 统计量、渐近或有限样本条件、阈值与数值求解 |
| 谱色与消光相关背景 | [41] | 文献讨论的具体物理量是否与待支持断言相同 |

表中的分组是阅读任务，不能据此断言每篇已支持某个公式。参考代码须另记实际版本/commit、文件、许可和 ACSD 采用的部分，不按项目名称推断代码可复制或生产已调用。

## 科学解释与证据

输出为线性图像，星等是派生量；实际数值可以是原生计数、相对标度或已定义响应积分单位。合成积分常因子可由拟合系数吸收，不表示其单位和响应归一无需声明。未测项如何影响误差或上界，依固定模板、协方差和 PSD 缺项条件判断，不能一律称保守上限。

本页不额外规定参考通带、孔径生产政策或绝对标度承诺。同主题的原始研究注记完整保存在[来源记录](../../../../实验/documentation-refactor/docs/PHOTOMETRY_REFERENCE_NOTES_SOURCE.md)。其中“产物只以星等表达”“未测项不加而上限偏严”等表述与现行正本不能直接并用，已纳入[待核验问题](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)；保留来源不表示采用该结论。

## 参考文献与代码入口

以下条目保留来源书目和链接，仍需逐项核对原文、书目及实际适用范围。

[1] ESA/DPAC. Gaia DR3 Documentation: xp_sampled_mean_spectrum. https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_spectroscopic_tables/ssec_dm_xp_sampled_mean_spectrum.html

[2] ESA/DPAC. GaiaXPy 官方文档. https://gaia-dpci.github.io/GaiaXPy-website/

[3] Montegriffo P. 等. Gaia Data Release 3: External calibration of BP/RP low-resolution spectroscopic data. A&A, 2023, 674: A3. https://doi.org/10.1051/0004-6361/202243880

[4] De Angeli F. 等. Gaia Data Release 3: Processing and validation of BP/RP low-resolution spectral data. A&A, 2023, 674: A2. https://doi.org/10.1051/0004-6361/202243680

[5] Carrasco J. M. 等. Internal calibration of Gaia BP/RP low-resolution spectra. A&A, 2021, 652: A86. https://doi.org/10.1051/0004-6361/202141249

[6] Gaia Collaboration 等. Gaia Data Release 3: Summary of the content and survey properties. A&A, 2023, 674: A1. https://doi.org/10.1051/0004-6361/202243940

[7] Riello M. 等. Gaia Early Data Release 3: Photometric content and validation. A&A, 2021, 649: A3. https://doi.org/10.1051/0004-6361/202039587

[8] Bessell M. S. UBVRI passbands. PASP, 1990, 102: 1181–1199. https://doi.org/10.1086/132749

[9] Bessell M. S., Murphy S. Spectrophotometric libraries, revised photonic passbands, and zero points. PASP, 2012, 124: 140–172. https://doi.org/10.1086/664083

[10] Fukugita M. 等. The Sloan Digital Sky Survey photometric system. AJ, 1996, 111: 1748–1756. https://doi.org/10.1086/117915

[11] Sirianni M. 等. The photometric performance and calibration of the HST Advanced Camera for Surveys. PASP, 2005, 117: 1049–1112. https://doi.org/10.1086/444553

[12] STScI. synphot 官方文档. https://synphot.readthedocs.io/en/latest/

[13] SVO Filter Profile Service. http://svo2.cab.inta-csic.es/theory/fps/

[14] Oke J. B., Gunn J. E. Secondary standard stars for absolute spectrophotometry. ApJ, 1983, 266: 713–717. https://doi.org/10.1086/160817

[15] Oke J. B. Faint spectrophotometric standard stars. AJ, 1990, 99: 1621–1631. https://doi.org/10.1086/115444

[16] Blanton M. R., Roweis S. K-corrections and filter transformations in the ultraviolet, optical, and near-infrared. AJ, 2007, 133: 734–754. https://doi.org/10.1086/510127

[17] Bohlin R. C. 等. HST CALSPEC flux standards: Sirius (and Vega). AJ, 2014, 147: 127. https://doi.org/10.1088/0004-6256/147/6/127

[18] Bohlin R. C., Hubeny I., Rauch T. New grids of pure-hydrogen white dwarf NLTE model atmospheres and the HST/STIS flux calibration. AJ, 2020, 160: 21. https://doi.org/10.3847/1538-3881/ab94b4

[19] STScI. CALSPEC 官方页. https://www.stsci.edu/hst/instrumentation/reference-data-for-calibration-and-tools/astronomical-catalogs/calspec

[20] Bertin E. Automatic astrometric and photometric calibration with SCAMP. ASP Conf. Ser., 2006, 351: 112.

[21] Lang D. 等. Astrometry.net: Blind astrometric calibration of arbitrary astronomical images. AJ, 2010, 139: 1782–1800. https://doi.org/10.1088/0004-6256/139/5/1782

[22] Bertin E., Arnouts S. SExtractor: Software for source extraction. A&AS, 1996, 117: 393–404. https://doi.org/10.1051/aas:1996164

[23] Barbary K. SEP: Source Extractor as a library. JOSS, 2016, 1: 58. https://doi.org/10.21105/joss.00058

[24] photutils 开发团队. photutils 参考实现（BSD-3-Clause）. https://github.com/astropy/photutils

[25] Astropy Collaboration 等. Astropy: A community Python package for astronomy. A&A, 2013, 558: A33. https://doi.org/10.1051/0004-6361/201322068

[26] Bertin E. SWarp: Resampling and co-adding FITS images together. https://www.astromatic.net/software/swarp/

[27] Stetson P. B. DAOPHOT: A computer program for crowded-field stellar photometry. PASP, 1987, 99: 191–222. https://doi.org/10.1086/131977

[28] Irwin M. J. The automatic analysis of starfield photographs. MNRAS, 1985, 214: 575–604. https://doi.org/10.1093/mnras/214.4.575

[29] Anderson J., King I. R. Toward high-precision astrometry with WFPC2. PASP, 2000, 112: 1360–1382. https://doi.org/10.1086/316632

[30] Mortara L., Fowler A. Solid state imagers for astronomy: The performance of charge-coupled devices at optical and near infrared wavelengths. SPIE, 1981, 290: 28–33. https://doi.org/10.1117/12.965833

[31] Merline W. J., Howell S. B. A realistic model for point-sources imaged on array detectors. Exp. Astron., 1995, 6: 163–210. https://doi.org/10.1007/BF00421131

[32] Horne K. An optimal extraction algorithm for CCD spectroscopy. PASP, 1986, 98: 609–617. https://doi.org/10.1086/131801

[33] Zackay B., Ofek E. O. How to coadd images. ApJ, 2017, 836: 187. https://doi.org/10.3847/1538-4357/836/2/187

[34] Stubbs C. W., Tonry J. L. Toward 1% photometry: End-to-end calibration of astronomical telescopes and detectors. ApJ, 2006, 646: 1436–1444. https://doi.org/10.1086/505138

[35] Regnault N. 等. Photometric calibration of the Supernova Legacy Survey fields. A&A, 2009, 506: 999–1042. https://doi.org/10.1051/0004-6361/200912446

[36] Padmanabhan N. 等. An improved photometric calibration of the Sloan Digital Sky Survey imaging data. ApJ, 2008, 674: 1217–1233. https://doi.org/10.1086/524677

[37] Starck J.-L., Murtagh F. Automatic noise estimation from the multiresolution support. PASP, 1998, 110: 193–199. https://doi.org/10.1086/316124

[38] Maples M. P. 等. Robust Chauvenet outlier rejection. ApJS, 2018, 238: 2. https://doi.org/10.3847/1538-4365/aad23d

[39] Burke D. L. 等. Forward global photometric calibration of the Dark Energy Survey. AJ, 2018, 155: 41. https://doi.org/10.3847/1538-3881/aa9f22

[40] Schlafly E. F. 等. Photometric calibration of the first 1.5 years of the Pan-STARRS1 survey. ApJ, 2012, 756: 158. https://doi.org/10.1088/0004-637X/756/2/158

[41] Schlafly E. F., Finkbeiner D. P. Measuring reddening with Sloan Digital Sky Survey stellar spectra and recalibrating SFD. ApJ, 2011, 737: 103. https://doi.org/10.1088/0004-637X/737/2/103

[42] Rousseeuw P. J., Croux C. Alternatives to the median absolute deviation. JASA, 1993, 88: 1273–1283. https://doi.org/10.1080/01621459.1993.10476408

[43] Beaton A. E., Tukey J. W. The fitting of power series, meaning polynomials, illustrated on band-spectroscopic data. Technometrics, 1974, 16: 147–185. https://doi.org/10.1080/00401706.1974.10489171

[44] Marquardt D. W. An algorithm for least-squares estimation of nonlinear parameters. J. Soc. Indust. Appl. Math., 1963, 11: 431–441. https://doi.org/10.1137/0111030
