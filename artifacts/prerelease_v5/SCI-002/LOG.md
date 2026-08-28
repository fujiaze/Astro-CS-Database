# SCI-002 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS SCI-002 行; docs/science/{ASTROMETRY,PHOTOMETRY,PSF}.md(T101/T102/T103 冻结版); SCI-001 引入的 science_contract_lint。

## 动作
1. 差距分析: lint 显示三份合同各缺 坐标 frame/专属问题/Primary literature/Acceptance 四节(S2/S3 全过)。
2. 文献核验(web_search 实证): Greisen & Calabretta 2002 A&A 395 1061 DOI 10.1051/0004-6361:20021326(A&A 全文页确认); Calabretta & Greisen 2002 A&A 395 1077(A&A 全文页确认); Tukey biweight c=4.685=95% 高斯效率(PMC6768164 原文语句核对+Mosteller & Tukey 1977); Moffat 1969 A&A 3 455(bibcode 级)。Shupe 2005 SIP 与 Gaia DR3 为 bibcode 级定位并显式标注"未逐页核验"。
3. 三份合同各补四节: 3a frame(ICRS/J2000 或像素域声明)/9a 专属问题逐项回答(WCS 约定/PSF 参数/aperture-flux-background 边界/scale 与不确定度)/14 文献(定位+限制声明)/15 Acceptance(Oracle 门+lint+SYN-002 转换映射)。
4. 关键消歧: 本链为 PSF 拟合通量域,孔径测光显式排除(引入须新 claim);q_psf 非 SNR;WCS 残差不并入光度不确定度。

## 验证
- SCIENCE_CONTRACT_LINT_PASS files=3 sections=15。
- 全量回归 unittest **19/19 OK**; VERSION_CONSISTENCY_PASS; GLOSSARY_PASS。

## 产物
docs/science/{ASTROMETRY,PHOTOMETRY,PSF}.md 各补四节; 本日志。

## PASS 判定
坐标与 photometry 语义不依赖代码猜测(锚点齐); 可构造解析星场(§15 SYN-002 映射); 文献定位核验。SCI-002 = PASS。
