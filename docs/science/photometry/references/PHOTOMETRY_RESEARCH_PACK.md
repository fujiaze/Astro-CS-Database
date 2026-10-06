# 测光文献注记

本注记是测光分册的文献研究部分：把盖亚 XP 光谱、合成测光通带、星等系统与星表引导测光等一手出处按主题登记，说明每条文献支撑测光链的哪一环。它不定义公式、常数与判据，权威口径见同目录的测光拟合分册。

## 1 盖亚 XP 光谱的表示与定标

盖亚 DR3 的低分辨率光谱以基函数连续表示与采样表示两种形态发布，二者不是同一产品。本链消费的是外部定标的采样表示，其采样网格与通量单位由官方数据模型给出 [1]，连续表示到采样表示的互转由官方工具说明 [2]。外定标的仪器响应模型与谱重建见 [3]，处理与验证链见 [4]，内定标见 [5]。星表总览与发布内容见 [6]，早期发布的积分光度内容与验证见 [7]。

引用边界：连续表示是内部系统，不携带物理刻度；只有采样表示的谱辐照度进入正向合成。对绝对定标推导本身，本注记只登记出处，不重述其推导。

## 2 合成测光与系统响应

正向合成的方法学是标准做法：恒星光谱与系统响应在波长上积分。通带定义的经典文献给出滤光片与探测器合成透过率以及能量计数与光子计数的口径区分 [8]；光子计数口径的修正通带与零点见 [9]；成像巡天系统响应与绝对零点的完整范例见 [10]；端到端系统透过率与光谱相乘的工程范例见 [11]。公开实现规范见合成测光工具文档 [12]，滤镜与仪器响应曲线的公开库见 [13]。

对本链的约束有两条。第一，探测器量子效率是通带的组成部分，未配置时按恒一处理属于显式未建模项。第二，合成通量的绝对归一常数被标定因子吸收，不得当作科学量引用。

## 3 星等系统与绝对通量刻度

星等换算的标准式为 `m = −2.5·log10(F) + ZP`，零点依赖系统与通带。AB 星等系统的原始文献见 [14]，绝对分光测光的标准星工作见 [15]，甚弱分光标准星网络见 [16]。现代绝对通量刻度的一手依据是空间望远镜的白矮星标准与定标链 [17][18]，官方标准星光谱交付面见 [19]。

对本链的约束：各条绝对零点的用途限于参考系语义与误差预算来源；本链产物只以星等与相对星等表达，不宣称绝对通量刻度。

## 4 星表引导检测与天体测量对照

本链的范式是星表引导：用本帧坐标解算把盖亚星表逆投影到像素域，只在星表位置做质心与点扩散函数拟合。相对光度与天体测量联合解算的工程结构见 [20]；盲解加星表精化的完整开源对照见 [21]；分块背景网格加检测加孔径测光的工程实现见 [22]，其库化实现见 [23]；点扩散函数测光与二维背景的参考实现见 [24]；坐标投影互转的独立实现见 [25]；逐像素加权组合与背景统计的开源对照见 [26]。拥挤场点扩散函数测光的方法学源头见 [27][28][29]。

引用边界：孔径测光在本链只作诊断与交叉验证，生产口径为点扩散函数拟合域。传染性许可的代码只读、不复制入仓。

## 5 误差预算的文献构成

测光分册的逐项预算由下列文献族支撑：光子噪声与读出模型 [30][31]；点扩散函数拟合不确定度 [27][28][29]；最优提取与信息下界 [32][33]；平场与大尺度响应残余 [34][35][36]；天光背景估计残余 [22][37][38]；颜色与通带失配见合成测光与 XP 定标各条；星等定标链误差 [9][39][40][17][18]；大气消光的标准处理见 [41]，本链将其列为未建模项；稳健统计与离群处理 [42][43][38]；非线性最小二乘 [44]。

本注记不给任何预算数值；数值推导属实验单元。未测项按不加处理，上限偏严。

## 6 未找到一手出处的条目

相对光度联合解算的会议论文 [20] 的配套方法学论文只有会议集级定位，未见数字标识与预印本，方法学对照改用其官方源码面。仪器量子效率曲线与 XP 谱覆盖之外的行为无一手出处，前者属数据面缺口、按未建模项处理，后者按网格外无数据、不外推处理。

## 参考文献与参考代码

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
