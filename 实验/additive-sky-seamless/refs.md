# P5 加性天光去除与无接缝叠加 · 文献核验台账（只收一手 VERIFIED 条目）

**口径**：本台账只收经独立审计三路核验、且被《分歧台账》（独立审计/实验重做/总编对账/分歧台账.md）
或《五单元成稿简报》采信为 **VERIFIED** 的一手条目。核验渠道与结论沿用
`独立审计/实验重做/P5加性天光去除/{路线1,路线2,路线3,补实验-5.07与relstep}/refs.md`，
引用面订正按 A-P5-09 执行。标注级（非 VERIFIED）条目单列于末节，不进入论文引用面。

---

## VERIFIED 条目

### R1 · Huber, P. J. 1964
- **锚**: *Robust Estimation of a Location Parameter*, Ann. Math. Statist. **35**(1), 73–101. **DOI 10.1214/aoms/1177703732**
- **核验方式**: Crossref + Project Euclid 元数据逐项吻合（路线1/路线2 双路独立核验）。
- **用途**: 稳健位置 M-估计总纲；论文判据面中 Huber 框架的出处。**不含 δ=1.345 数值归属**（该归属在 R2）。

### R2 · Holland, P. W. & Welsch, R. E. 1977
- **锚**: *Robust regression using iteratively reweighted least-squares*, Comm. Statist. **A6**, 813–827. **DOI 10.1080/03610927708827533**
- **核验方式**: Crossref 元数据级（路线2）；取值表原文在线不可得，但 **δ=1.345 的数值由路线2 e5 以闭式渐近效率方程独立复算**（δ(0.95)=1.3449975），不依赖该表逐页。
- **用途**: δ=1.345 的归属锚（A-P5-09 订正：不归 Huber 1964）。

### R3 · Andrae, R., Schulze-Hartung, T. & Melchior, P. 2010
- **锚**: *Dos and don'ts of reduced chi-squared*. **arXiv:1012.3754**
- **核验方式**: arXiv abs + PDF 全文直取，式 (9) K = N − P_eff = N − rank(X) **逐字核验**（路线1/路线2 双路）。
- **用途**: E[χ²] = n_obs − r_eff 的文献腿；A-P5-01 方向词订正（χ²_red 以 n_params 为分母时**高估**）的理论依据。

### R4 · Fruchter, A. S. & Hook, R. N. 2002
- **锚**: *Drizzle: A Method for the Linear Reconstruction of Undersampled Images*, PASP **114**, 144. **DOI 10.1086/338393**；arXiv:astro-ph/9808087
- **核验方式**: 一手 PDF 全文（路线1，PASP 页码逐项）+ arXiv abs 摘要自述（路线2）。
- **用途**: **仅引其输出像素非独立（噪声相关）论断**，以证 k_corr 修正的必要性；该文**不含** k_corr、1.3883 或 1.4（D-08 全文子串级检索确认，D-03 撤换『§7.2 式(7)后』错误出处）。

### R5 · Clopper, C. J. & Pearson, E. S. 1934
- **锚**: *The use of confidence or fiducial limits illustrated in the case of the binomial*, Biometrika **26**(4), 404–413. **DOI 10.1093/biomet/26.4.404**
- **核验方式**: Crossref（路线1）。
- **用途**: 接缝门检出率/虚警率的置信区间口径（路线2 e3 区间算法与该文一致的核验见路线2 refs）。

### R6 · Padmanabhan, N., Schlegel, D. J., Finkbeiner, D. P., et al. 2008
- **锚**: *An Improved Photometric Calibration of the SDSS*, ApJ **674**, 1217. **arXiv:astro-ph/0703454**；DOI 10.1086/524677
- **核验方式**: 一手核验＋**引用号订正**（A-P5-09：arXiv:0805.2366 实为 Ivezic et al. LSST 综述，不得混用）。
- **用途**: 重叠观测联合相对定标（ubercalibration）先例——帧间乘性差必须在上游（Phase1）吸收的方法学支撑。

### R7 · Gruen, D., Seitz, S. & Bernstein, G. M. 2014
- **锚**: *Implementation of Robust Image Artifact Removal in SWarp through Clipped Mean Stacking*, PASP **126**, 158. **DOI 10.1086/675080**；arXiv:1401.4169
- **核验方式**: 一手核验，**主题收窄**（A-P5-09）：内容为多帧叠加稳健伪迹剔除。
- **用途**: 仅作 SWarp 稳健叠加先例；**不得作背景匹配/背景定标依据**。

### R8 · Casertano, S. et al. 2000
- **锚**: *WFPC2 Observations of the Hubble Deep Field-South*, AJ **120**, 2747. arXiv:astro-ph/0010245；DOI 10.1086/316851
- **核验方式**: 一手核验（路线1）。
- **用途**: drizzle 输出噪声性质的先声（背景引用；F&H 2002 正式引用对象）。

### R9 · Serfling, R. J. 1980
- **锚**: *Approximation Theorems of Mathematical Statistics*, Wiley. **DOI 10.1002/9780470316481**（ISBN 0-471-02403-1）
- **核验方式**: Crossref 书目级（路线1/路线2）。**章内小节号 UNRESOLVED**（在线不可得）——论文中**标注级引用**，结论由 MC 独立复现承担，不依赖 pinpoint。
- **用途**: 阶统计量渐近理论（中位数方差渐近式 π/2·σ²/N）的书目锚。

---

## 标注级条目（不作证据使用，仅定位）

| 条目 | 状态 | 处理 |
|---|---|---|
| Kendall, *The Advanced Theory of Statistics* Vol. 1, Example 9.7（中位数方差） | 书目级，archive.org OCR 二手承担 | 不作 pinpoint 证据；数值由精确阶统计量积分 [推导]＋MC [实验] 承担 |
| Nocedal & Wright, *Numerical Optimization*（步长控制=数值工程） | 定位级 | 支撑 rel_step_max=0.1 反事实读法豁免（补实验 C2），不入引用面 |
| SExtractor BACK_SIZE 网格尺度语义（Bertin 1996/2010） | 定位级 | 补实验实验 A 背景口径对照，不入引用面 |
| Duchon 1977 / Wahba 1990（薄板样条正则） | 定位级 | smoothing_lambda 讨论背景，不入引用面 |
| Huber & Ronchetti 2009, *Robust Statistics* 2nd ed.（DOI 10.1002/9780470434697） | 元数据 VERIFIED，书内页码 UNRESOLVED | 定位级引用 |

## 明确排除（不列条目、不引用）

- **Tikhonov 1963**：题录级未核（UNRESOLVED），不作依据。
- **quality_factor 0.1/0.5、min_samples=5、IRLS tolerance、σ_floor=1e-3、max_nodes**：豁免清单——项目约定/工程合同/数值防护，**不注文献出处**（负责人已批；A-P5-12）。
- 原 REPORT_paper 引用的『k_corr=1.4 文献锚』：**不存在**——k_corr 不含于任何外部文献，改为两因子查表（D-08，P3 单元承载）。
